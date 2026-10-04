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
#include <vector>

#include <raylib.h>
#include <rlgl.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Input.h>

#include "preview_host.h"
#include "scene_watch.h"

static constexpr const char WINDOW_TITLE[] = "mini UI preview";
static constexpr int INITIAL_WIDTH = 1280;
static constexpr int INITIAL_HEIGHT = 720;
static constexpr int TARGET_FPS = 60;
static constexpr double DEFAULT_SETTLE_SECONDS = 1.0;

struct Options {
	std::vector<std::filesystem::path> scenes;
	std::optional<std::filesystem::path> shots;
	double settle = DEFAULT_SETTLE_SECONDS;
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

static Color RaylibColour(Rml::Colourb colour)
{
	return {colour.red, colour.green, colour.blue, colour.alpha};
}

/* The scene lives behind a pointer that stays put, because its data models point into it. */
static std::unique_ptr<Scene> Present(PreviewHost &host, const std::filesystem::path &file)
{
	std::optional<Scene> loaded = LoadScene(file);
	if (!loaded.has_value()) return nullptr;

	auto scene = std::make_unique<Scene>(std::move(*loaded));
	SetWindowSize(scene->size.x, scene->size.y);
	SetWindowTitle(file.stem().string().c_str());
	host.Show(*scene);
	return scene;
}

/* raylib's batch goes out first and RmlUi draws over it, the order the game composites in. */
static void DrawFrame(PreviewHost &host, const Scene &scene, Image *capture = nullptr)
{
	BeginDrawing();
	ClearBackground(RaylibColour(scene.backdrop));
	rlDrawRenderBatchActive();
	host.Render(scene.size);
	if (capture != nullptr) *capture = LoadImageFromScreen();
	EndDrawing();
}

static void ForwardInput(Rml::Context &context)
{
	int modifiers = (IsKeyDown(KEY_LEFT_CONTROL) ? Rml::Input::KM_CTRL : 0) | (IsKeyDown(KEY_LEFT_SHIFT) ? Rml::Input::KM_SHIFT : 0);
	Vector2 mouse = GetMousePosition();
	context.ProcessMouseMove(static_cast<int>(mouse.x), static_cast<int>(mouse.y), modifiers);
	for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_MIDDLE; button++) {
		if (IsMouseButtonPressed(button)) context.ProcessMouseButtonDown(button, modifiers);
		if (IsMouseButtonReleased(button)) context.ProcessMouseButtonUp(button, modifiers);
	}
	float wheel = GetMouseWheelMove();
	if (wheel != 0.0f) context.ProcessMouseWheel(Rml::Vector2f(0.0f, -wheel), modifiers);
}

/* Page up and down walk the scenes; saving any document, stylesheet, icon or scene redraws at once. */
static void Browse(PreviewHost &host, const std::vector<std::filesystem::path> &files)
{
	size_t index = 0;
	std::unique_ptr<Scene> scene = Present(host, files[index]);
	SceneWatch watch({std::filesystem::path(MEDIA_DIR) / "mini_ui", PREVIEW_SCENE_DIR});

	while (!WindowShouldClose()) {
		int step = (IsKeyPressed(KEY_PAGE_DOWN) ? 1 : 0) - (IsKeyPressed(KEY_PAGE_UP) ? 1 : 0);
		index = (index + files.size() + step) % files.size();
		if (step != 0 || IsKeyPressed(KEY_F5) || watch.Changed()) {
			if (std::unique_ptr<Scene> next = Present(host, files[index]); next != nullptr) scene = std::move(next);
		}
		if (scene == nullptr) {
			BeginDrawing();
			EndDrawing();
			continue;
		}
		ForwardInput(*host.Context());
		DrawFrame(host, *scene);
	}
}

/* The pointer goes where the scene puts it, and presses once the documents have a layout to hit. */
static void Point(Rml::Context &context, const Scene &scene, bool press_now)
{
	if (!scene.pointer.has_value()) return;
	context.ProcessMouseMove(static_cast<int>(scene.pointer->x), static_cast<int>(scene.pointer->y), 0);
	if (press_now && scene.press) context.ProcessMouseButtonDown(MOUSE_BUTTON_LEFT, 0);
}

/* Each scene runs long enough for its animations to finish before it is saved. */
static bool Shoot(PreviewHost &host, const std::vector<std::filesystem::path> &files, const std::filesystem::path &directory, double settle)
{
	std::filesystem::create_directories(directory);
	bool all_saved = true;
	for (const std::filesystem::path &file : files) {
		std::unique_ptr<Scene> scene = Present(host, file);
		if (scene == nullptr) {
			all_saved = false;
			continue;
		}

		double start = GetTime();
		for (int frame = 0; GetTime() - start < settle; frame++) {
			Point(*host.Context(), *scene, frame == 1);
			DrawFrame(host, *scene);
		}

		Image image;
		DrawFrame(host, *scene, &image);
		std::filesystem::path shot = directory / file.stem().replace_extension(".png");
		all_saved &= ExportImage(image, shot.string().c_str());
		UnloadImage(image);
	}
	return all_saved;
}

int main(int argc, char **argv)
{
	Options options = ReadOptions(argc, argv);
	if (options.scenes.empty()) {
		std::fprintf(stderr, "no scene files\n");
		return 1;
	}

	SetTraceLogLevel(LOG_WARNING);
	if (options.shots.has_value()) SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(INITIAL_WIDTH, INITIAL_HEIGHT, WINDOW_TITLE);
	SetTargetFPS(TARGET_FPS);

	int result = 0;
	{
		PreviewHost host(MEDIA_DIR);
		if (!host.Start()) {
			result = 1;
		} else if (options.shots.has_value()) {
			result = Shoot(host, options.scenes, *options.shots, options.settle) ? 0 : 1;
		} else {
			Browse(host, options.scenes);
		}
	}
	CloseWindow();
	return result;
}
