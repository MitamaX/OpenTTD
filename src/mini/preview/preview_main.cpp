/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file preview_main.cpp The mini UI documents drawn by the same engine, fonts and scale as the game, from scene files instead of a running game. */

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <RmlUi_Include_GL3.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <png.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Input.h>

#include "preview_host.h"
#include "scene_watch.h"

static constexpr const char WINDOW_TITLE[] = "mini UI preview";
static constexpr int INITIAL_WIDTH = 1280;
static constexpr int INITIAL_HEIGHT = 720;
static constexpr int GL_MAJOR = 3;
static constexpr int GL_MINOR = 3;
static constexpr int SWAP_ON_VSYNC = 1;
static constexpr double DEFAULT_SETTLE_SECONDS = 1.0;
static constexpr float CHANNEL_MAX = 255.0f;
static constexpr int RGBA_BYTES = 4;

struct Options {
	std::vector<std::filesystem::path> scenes;
	std::optional<std::filesystem::path> shots;
	double settle = DEFAULT_SETTLE_SECONDS;
};

/* What the window callbacks hand over to the browse loop. */
struct WindowInput {
	PreviewHost *host = nullptr;
	int step = 0;
	bool reload = false;
};

static Options ReadOptions(int argc, char **argv)
{
	Options options;
	for (int i = 1; i < argc; i++) {
		std::string_view argument = argv[i];
		if (argument == "--shots" && i + 1 < argc) {
			options.shots = argv[++i];
		} else if (argument == "--settle" && i + 1 < argc) {
			options.settle = std::stod(argv[++i]);
		} else {
			options.scenes.emplace_back(argument);
		}
	}
	if (options.scenes.empty()) options.scenes = SceneFiles(PREVIEW_SCENE_DIR);
	return options;
}

static WindowInput &InputOf(GLFWwindow *window)
{
	return *static_cast<WindowInput *>(glfwGetWindowUserPointer(window));
}

static int Modifiers(int mods)
{
	return ((mods & GLFW_MOD_CONTROL) != 0 ? Rml::Input::KM_CTRL : 0) | ((mods & GLFW_MOD_SHIFT) != 0 ? Rml::Input::KM_SHIFT : 0);
}

static int HeldModifiers(GLFWwindow *window)
{
	bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
	bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
	return Modifiers((ctrl ? GLFW_MOD_CONTROL : 0) | (shift ? GLFW_MOD_SHIFT : 0));
}

static Rml::Context *ContextOf(GLFWwindow *window)
{
	return InputOf(window).host->Context();
}

/* GLFW numbers the left, right and middle buttons the way RmlUi does. */
static void OnButton(GLFWwindow *window, int button, int action, int mods)
{
	Rml::Context *context = ContextOf(window);
	if (context == nullptr || button > GLFW_MOUSE_BUTTON_MIDDLE) return;
	if (action == GLFW_PRESS) {
		context->ProcessMouseButtonDown(button, Modifiers(mods));
	} else if (action == GLFW_RELEASE) {
		context->ProcessMouseButtonUp(button, Modifiers(mods));
	}
}

static void OnMove(GLFWwindow *window, double x, double y)
{
	if (Rml::Context *context = ContextOf(window); context != nullptr) context->ProcessMouseMove(static_cast<int>(x), static_cast<int>(y), HeldModifiers(window));
}

static void OnScroll(GLFWwindow *window, double, double y)
{
	if (Rml::Context *context = ContextOf(window); context != nullptr) context->ProcessMouseWheel(Rml::Vector2f(0.0f, static_cast<float>(-y)), HeldModifiers(window));
}

/* Page up and down walk the scenes; F5 reads the current one again. */
static void OnKey(GLFWwindow *window, int key, int, int action, int)
{
	if (action != GLFW_PRESS) return;
	WindowInput &input = InputOf(window);
	if (key == GLFW_KEY_PAGE_DOWN) input.step++;
	if (key == GLFW_KEY_PAGE_UP) input.step--;
	if (key == GLFW_KEY_F5) input.reload = true;
}

/* The scene lives behind a pointer that stays put, because its data models point into it. */
static std::unique_ptr<Scene> Present(GLFWwindow *window, PreviewHost &host, const std::filesystem::path &file)
{
	std::optional<Scene> loaded = LoadScene(file);
	if (!loaded.has_value()) return nullptr;

	auto scene = std::make_unique<Scene>(std::move(*loaded));
	glfwSetWindowSize(window, scene->size.x, scene->size.y);
	glfwSetWindowTitle(window, file.stem().string().c_str());
	host.Show(*scene);
	return scene;
}

/* The backdrop goes down first and RmlUi draws over it, the order the game composites in. */
static void DrawFrame(PreviewHost &host, const Scene &scene)
{
	glViewport(0, 0, scene.size.x, scene.size.y);
	glClearColor(scene.backdrop.red / CHANNEL_MAX, scene.backdrop.green / CHANNEL_MAX, scene.backdrop.blue / CHANNEL_MAX, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	host.Render(scene.size);
}

/* The framebuffer's rows run bottom up, so the image is written with a negative stride. */
static bool SaveFrame(Rml::Vector2i size, const std::filesystem::path &file)
{
	std::vector<png_byte> pixels(static_cast<size_t>(size.x) * size.y * RGBA_BYTES);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	image.width = static_cast<png_uint_32>(size.x);
	image.height = static_cast<png_uint_32>(size.y);
	image.format = PNG_FORMAT_RGBA;
	png_int_32 stride = -static_cast<png_int_32>(size.x * RGBA_BYTES);
	return png_image_write_to_file(&image, file.string().c_str(), 0, pixels.data(), stride, nullptr) != 0;
}

/* Saving any document, stylesheet, icon or scene redraws at once. */
static void Browse(GLFWwindow *window, PreviewHost &host, const std::vector<std::filesystem::path> &files)
{
	WindowInput &input = InputOf(window);
	size_t index = 0;
	std::unique_ptr<Scene> scene = Present(window, host, files[index]);
	SceneWatch watch({std::filesystem::path(MEDIA_DIR) / "mini_ui", PREVIEW_SCENE_DIR});

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		int step = std::exchange(input.step, 0);
		index = (index + files.size() + step) % files.size();
		if (step != 0 || std::exchange(input.reload, false) || watch.Changed()) {
			if (std::unique_ptr<Scene> next = Present(window, host, files[index]); next != nullptr) scene = std::move(next);
		}
		if (scene != nullptr) DrawFrame(host, *scene);
		glfwSwapBuffers(window);
	}
}

/* The pointer goes where the scene puts it, and presses once the documents have a layout to hit. */
static void Point(Rml::Context &context, const Scene &scene, bool press_now)
{
	if (!scene.pointer.has_value()) return;
	context.ProcessMouseMove(static_cast<int>(scene.pointer->x), static_cast<int>(scene.pointer->y), 0);
	if (press_now && scene.press) context.ProcessMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT, 0);
}

/* Each scene runs long enough for its animations to finish before it is saved. */
static bool Shoot(GLFWwindow *window, PreviewHost &host, const std::vector<std::filesystem::path> &files, const std::filesystem::path &directory, double settle)
{
	std::filesystem::create_directories(directory);
	bool all_saved = true;
	for (const std::filesystem::path &file : files) {
		std::unique_ptr<Scene> scene = Present(window, host, file);
		if (scene == nullptr) {
			all_saved = false;
			continue;
		}

		double start = glfwGetTime();
		for (int frame = 0; glfwGetTime() - start < settle; frame++) {
			glfwPollEvents();
			Point(*host.Context(), *scene, frame == 1);
			DrawFrame(host, *scene);
			glfwSwapBuffers(window);
		}

		DrawFrame(host, *scene);
		all_saved &= SaveFrame(scene->size, directory / file.stem().replace_extension(".png"));
		glfwSwapBuffers(window);
	}
	return all_saved;
}

static GLFWwindow *OpenWindow(bool hidden)
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_MINOR);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_VISIBLE, hidden ? GLFW_FALSE : GLFW_TRUE);
	GLFWwindow *window = glfwCreateWindow(INITIAL_WIDTH, INITIAL_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
	if (window == nullptr) return nullptr;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(SWAP_ON_VSYNC);
	glfwSetMouseButtonCallback(window, OnButton);
	glfwSetCursorPosCallback(window, OnMove);
	glfwSetScrollCallback(window, OnScroll);
	glfwSetKeyCallback(window, OnKey);
	return window;
}

static int Run(GLFWwindow *window, const Options &options)
{
	PreviewHost host(MEDIA_DIR);
	WindowInput input{&host};
	glfwSetWindowUserPointer(window, &input);
	if (!host.Start()) return 1;
	if (options.shots.has_value()) return Shoot(window, host, options.scenes, *options.shots, options.settle) ? 0 : 1;
	Browse(window, host, options.scenes);
	return 0;
}

int main(int argc, char **argv)
{
	Options options = ReadOptions(argc, argv);
	if (options.scenes.empty()) {
		std::fprintf(stderr, "no scene files\n");
		return 1;
	}
	if (glfwInit() != GLFW_TRUE) {
		std::fprintf(stderr, "no window system\n");
		return 1;
	}

	GLFWwindow *window = OpenWindow(options.shots.has_value());
	int result = window == nullptr ? 1 : Run(window, options);
	if (window != nullptr) glfwDestroyWindow(window);
	glfwTerminate();
	return result;
}
