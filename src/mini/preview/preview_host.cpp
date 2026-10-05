/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file preview_host.cpp RmlUi set up the way the game sets it up, showing one scene at a time. */

#include "preview_host.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <RmlUi/Core.h>
#include <RmlUi_Renderer_GL3.h>

#include "../ui/fonts.h"
#include "../ui/model_tag.h"
#include "json_model.h"

static constexpr const char CONTEXT_NAME[] = "preview";

struct PreviewFace {
	std::vector<std::filesystem::path> candidates;
	Rml::Style::FontWeight weight;
};

/* Without the game there is no font search to ask, so the preview takes the
 * usual Hangul faces from wherever each platform keeps them. */
static std::vector<PreviewFace> PreviewFaces()
{
	using Rml::Style::FontWeight;
#if defined(_WIN32)
	const char *windows = std::getenv("WINDIR");
	if (windows == nullptr) return {};
	std::filesystem::path fonts = std::filesystem::path(windows) / "Fonts";
	return {{{fonts / "malgun.ttf"}, FontWeight::Normal}, {{fonts / "malgunbd.ttf"}, FontWeight::Bold}};
#elif defined(__APPLE__)
	return {{{"/System/Library/Fonts/AppleSDGothicNeo.ttc"}, FontWeight::Normal}};
#else
	return {
		{{"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc"}, FontWeight::Normal},
		{{"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc", "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc"}, FontWeight::Bold},
	};
#endif
}

/* Each face is the first of its candidates that is installed here, registered under the family the documents ask for. */
static void LoadFonts()
{
	for (const PreviewFace &face : PreviewFaces()) {
		auto installed = std::ranges::find_if(face.candidates, [](const std::filesystem::path &file) {
			std::error_code error;
			return std::filesystem::exists(file, error);
		});
		if (installed != face.candidates.end()) Rml::LoadFontFace(installed->string(), MINI_FONT_FAMILY, Rml::Style::FontStyle::Normal, face.weight);
	}
}

/* Documents are opened by the relative paths the game uses, so their links resolve the same way. */
Rml::FileHandle PreviewFiles::Open(const Rml::String &path)
{
	return reinterpret_cast<Rml::FileHandle>(std::fopen((this->root / path).string().c_str(), "rb"));
}

void PreviewFiles::Close(Rml::FileHandle file)
{
	std::fclose(reinterpret_cast<FILE *>(file));
}

size_t PreviewFiles::Read(void *buffer, size_t size, Rml::FileHandle file)
{
	return std::fread(buffer, 1, size, reinterpret_cast<FILE *>(file));
}

bool PreviewFiles::Seek(Rml::FileHandle file, long offset, int origin)
{
	return std::fseek(reinterpret_cast<FILE *>(file), offset, origin) == 0;
}

size_t PreviewFiles::Tell(Rml::FileHandle file)
{
	return static_cast<size_t>(std::ftell(reinterpret_cast<FILE *>(file)));
}

bool PreviewLog::LogMessage(Rml::Log::Type type, const Rml::String &message)
{
	if (type <= Rml::Log::LT_WARNING) std::fprintf(stderr, "[rmlui] %s\n", message.c_str());
	return true;
}

PreviewHost::PreviewHost(std::filesystem::path root) : files(std::move(root))
{
}

PreviewHost::~PreviewHost()
{
	if (this->renderer == nullptr) return;
	Rml::Shutdown();
	this->renderer.reset();
	RmlGL3::Shutdown();
}

bool PreviewHost::Start()
{
	Rml::String message;
	if (!RmlGL3::Initialize(&message)) {
		std::fprintf(stderr, "%s\n", message.c_str());
		return false;
	}

	auto renderer = std::make_unique<RenderInterface_GL3>();
	if (!*renderer) return false;
	this->renderer = std::move(renderer);

	Rml::SetFileInterface(&this->files);
	Rml::SetSystemInterface(&this->log);
	Rml::SetRenderInterface(this->renderer.get());
	Rml::Initialise();
	LoadFonts();
	return true;
}

/* Every show starts from a fresh context and empty caches, so edited files are read again. */
void PreviewHost::Show(Scene &scene)
{
	if (this->context != nullptr) Rml::RemoveContext(CONTEXT_NAME);
	Rml::Factory::ClearStyleSheetCache();
	Rml::Factory::ClearTemplateCache();
	Rml::ReleaseTextures();

	this->context = Rml::CreateContext(CONTEXT_NAME, scene.size);
	this->context->SetDensityIndependentPixelRatio(scene.dp);
	for (auto model = scene.models.begin(); model != scene.models.end(); ++model) BindJsonModel(*this->context, model.key(), model.value());
	for (const SceneDocument &entry : scene.documents) this->Open(entry, scene.styles);
}

void PreviewHost::Open(const SceneDocument &entry, const nlohmann::json &styles)
{
	Rml::String rml;
	if (!this->files.LoadFile(entry.path, rml)) {
		std::fprintf(stderr, "%s: cannot be read\n", entry.path.c_str());
		return;
	}
	if (!entry.model.empty()) rml = BindBodyToModel(std::move(rml), entry.model);

	Rml::ElementDocument *document = this->context->LoadDocumentFromMemory(rml, entry.path);
	if (document == nullptr) return;

	if (entry.position.has_value()) {
		document->SetProperty(Rml::PropertyId::Left, Rml::Property(entry.position->x, Rml::Unit::PX));
		document->SetProperty(Rml::PropertyId::Top, Rml::Property(entry.position->y, Rml::Unit::PX));
	}
	for (auto style = styles.begin(); style != styles.end(); ++style) {
		Rml::Element *element = document->GetElementById(style.key());
		if (element == nullptr) continue;
		for (auto property = style->begin(); property != style->end(); ++property) element->SetProperty(property.key(), property->get<Rml::String>());
	}
	document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}

void PreviewHost::Render(Rml::Vector2i size)
{
	this->context->Update();
	this->renderer->SetViewport(size.x, size.y);
	this->renderer->BeginFrame();
	this->context->Render();
	this->renderer->EndFrame();
}
