// Credit: https://learnopengl.com/In-Practice/Text-Rendering

#include <atomic>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <glm/gtx/string_cast.hpp>

#include "Log.h"
#include "Tileset.h"
#include "game/ClientGame.h"
#include "realm/Realm.h"
#include "ui/Canvas.h"
#include "ui/TextRenderer.h"

namespace Game3 {
	TextRenderer::TextRenderer(Canvas &canvas_): canvas(&canvas_), shader("TextRenderer") {
		shader.init(text_vert, text_frag);
	}

	TextRenderer::~TextRenderer() {
		remove();
	}

	void TextRenderer::remove() {
		if (initialized) {
			glDeleteVertexArrays(1, &vao);
			glDeleteBuffers(1, &vbo);
			vao = 0;
			vbo = 0;
			initialized = false;
		}
	}

	void TextRenderer::initRenderData() {
		if (initialized)
			return;

		auto freetypeLibrary = std::unique_ptr<FT_Library, FreeLibrary>(new FT_Library);
		if (FT_Init_FreeType(freetypeLibrary.get()))
			throw std::runtime_error("Couldn't initialize FreeType");

		auto freetypeFace = std::unique_ptr<FT_Face, FreeFace>(new FT_Face);
		if (FT_New_Face(*freetypeLibrary, "resources/CozetteVector.ttf", 0, freetypeFace.get()))
			throw std::runtime_error("Couldn't initialize font");

		auto &face = *freetypeFace;
		FT_Set_Pixel_Sizes(face, 0, 48);
		characters.clear();
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1); CHECKGL

		for (gunichar ch = 32; ch < 127; ++ch) {
			// Load character glyph
			if (FT_Load_Char(face, ch, FT_LOAD_RENDER))
				throw std::runtime_error("Failed to load glyph " + std::to_string(static_cast<uint32_t>(ch)));

			GLuint texture;
			glGenTextures(1, &texture); CHECKGL
			glBindTexture(GL_TEXTURE_2D, texture); CHECKGL
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer); CHECKGL
			// Set texture options
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); CHECKGL
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); CHECKGL
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); CHECKGL
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); CHECKGL
			// Store character for later use
			characters.emplace(ch, Character {
				texture,
				glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
				glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
				face->glyph->advance.x
			});
		}

		glGenVertexArrays(1, &vao); CHECKGL
		glGenBuffers(1, &vbo); CHECKGL
		glBindVertexArray(vao); CHECKGL
		glBindBuffer(GL_ARRAY_BUFFER, vbo); CHECKGL
		glBufferData(GL_ARRAY_BUFFER, 4 * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW); CHECKGL
		glEnableVertexAttribArray(0); CHECKGL
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0); CHECKGL
		glBindBuffer(GL_ARRAY_BUFFER, 0); CHECKGL
		glBindVertexArray(0); CHECKGL

		initialized = true;
		SUCCESS("TextRenderer::initRenderData() finished.");
	}

	void TextRenderer::update(int backbuffer_width, int backbuffer_height) {
		if (backbuffer_width != backbufferWidth || backbuffer_height != backbufferHeight) {
			backbufferWidth = backbuffer_width;
			backbufferHeight = backbuffer_height;
			projection = glm::ortho(0.f, static_cast<float>(backbuffer_width), static_cast<float>(backbuffer_height), 0.f, -1.f, 1.f);
			// projection = glm::ortho(0.f, 16.f, 16.f, 0.f, -1.f, 1.f);
			// projection = glm::ortho(0.f, static_cast<float>(backbuffer_width), 0.f, static_cast<float>(backbuffer_height), -1.f, 1.f);
			shader.bind();
			shader.set("projection", projection);
		}
	}

	void TextRenderer::drawOnMap(std::string_view text, float x, float y, TextAlign align, float scale, float angle, float alpha) {
		drawOnMap(text, TextRenderOptions {
			.x = x,
			.y = y,
			.scaleX = scale,
			.scaleY = scale,
			.angle = angle,
			.color = {0.f, 0.f, 0.f, alpha},
			.align = align,
		});
	}

	extern float xHax;

	void TextRenderer::drawOnMap(std::string_view text, TextRenderOptions options) {
		if (!initialized)
			initRenderData();

		auto &provider = canvas->game->activeRealm->tileProvider;
		const auto &tileset   = provider.getTileset(*canvas->game);
		const auto tile_size  = tileset->getTileSize();
		const auto map_length = CHUNK_SIZE * REALM_DIAMETER;

		auto &x = options.x;
		auto &y = options.y;
		// float x = 0;
		// float y = 0;
		auto &scale_x = options.scaleX;
		auto &scale_y = options.scaleY;
		// scale_x *= 1 / 16.f;
		// scale_y *= 1 / 16.f;

		scale_x *= canvas->scale;
		scale_y *= canvas->scale;

		auto tw = textWidth(text, scale_x);
		auto th = textHeight(text, scale_y);

		// x = 0;
		// y = 0;

		std::cout << "\e[31m(" << tw << ", " << th << ") tw,th \e[32m(" << centerX << ", " << centerY << ") cent \e[33m[" << canvas->scale << "] scale \e[34m(" << x << ", " << y << ") orig\e[39m\n";

		// x *= tile_size;
		// y *= tile_size;

		// x += canvas->width() / 2.f;
		// // x -= map_length * tile_size / 4.f;
		// // x += centerX * 4.f;

		// y += canvas->height() / 2.f;
		// // y -= map_length * tile_size / 4.f;
		// // y += centerY * 4.f;

		// x += centerX;
		// y -= centerY;

		// x *= canvas->scale;
		// y *= canvas->scale;





		// x *= tile_size * canvas->scale / 2.f;
		// y *= tile_size * canvas->scale / 2.f;

		// x += canvas->width() / 2.f;
		// x -= map_length * tile_size * canvas->scale / canvas->magic * 2.f; // TODO: the math here is a little sus... things might cancel out
		// x += centerX * canvas->scale * tile_size / 2.f;

		// y += canvas->height() / 2.f;
		// y -= map_length * tile_size * canvas->scale / canvas->magic * 2.f;
		// y += centerY * canvas->scale * tile_size / 2.f;


		// x *= canvas->scale;
		// y *= canvas->scale;


		// x *= canvas->scale / 2.f;
		// y *= canvas->scale / 2.f;

		x += map_length * tile_size;
		// y += map_length * tile_size;

		x += canvas->width() / 2.f;
		y += canvas->height() / 2.f;

		x *= canvas->scale;

		x -= map_length * tile_size / 2.f;

		x += centerX * 8.f * canvas->scale;
		y += centerY * 8.f * canvas->scale;

		// x *= canvas->scale;
		// y *= canvas->scale;


		auto whatX = 100.f / canvas->scale;
		whatX = 0.f;

		x += whatX;
		// y -= what;

		std::cout << "\e[33m" << whatX << "\e[39m\n";

		y = 100;


		// x /= 2.f;
		// y /= 2.f;




		// std::cout << "[1] " << x << " -> [2] ";
		// x += canvas->width() / 2.f;
		// y *= canvas->scale / 2.f;
		// // x *= canvas->scale;
		// // y *= canvas->scale;
		// std::cout << x << " -> [3] ";
		// x *= canvas->scale / 2.f;
		// std::cout << x << " -> [4] ";
		// // x -= map_length * tile_size * canvas->scale / canvas->magic * 2.f; // TODO: the math here is a little sus... things might cancel out
		// x -= map_length * canvas->scale / canvas->magic * 2.f;
		// std::cout << x << " -> [5] ";
		// x += centerX * canvas->scale * 2.f;
		// std::cout << x << " -> [6] ";
		// // x -= tw / 2.f;

		// y += canvas->height() / 2.f;
		// // y -= map_length * tile_size * canvas->scale / canvas->magic * 2.f;
		// y -= map_length * canvas->scale / canvas->magic * 2.f;
		// y -= centerY * canvas->scale * 2.f;
		// y -= th / 2.f;

		// x /= 16.f;
		// std::cout << x << ".\n";
		// y /= 16.f;



		// x = 100.0f;
		// y = 100.0f;

		std::cout << '(' << x << ", " << y << ") xy, (" << backbufferWidth << ", " << backbufferHeight << ") wh\n";
		v4 = {x, y, 0.f, 1.f};

		setupShader(text, options); CHECKGL

		glActiveTexture(GL_TEXTURE0); CHECKGL
		glBindVertexArray(vao); CHECKGL

		for (const char ch: text) {
			const auto &character = characters.at(ch);

			const float xpos = x + character.bearing.x * scale_x;
			const float ypos = y - (character.size.y - character.bearing.y) * scale_y;
			const float w = character.size.x * scale_x;
			const float h = character.size.y * scale_y;

			// Update VBO for each character
			const float vertices[6][4] = {
				{ xpos,     ypos + h,   0.0f, 0.0f },
				{ xpos,     ypos,       0.0f, 1.0f },
				{ xpos + w, ypos,       1.0f, 1.0f },

				{ xpos,     ypos + h,   0.0f, 0.0f },
				{ xpos + w, ypos,       1.0f, 1.0f },
				{ xpos + w, ypos + h,   1.0f, 0.0f }
			};

			// Render glyph texture over quad
			glBindTexture(GL_TEXTURE_2D, character.textureID); CHECKGL
			// Update content of VBO memory
			glBindBuffer(GL_ARRAY_BUFFER, vbo); CHECKGL
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); CHECKGL
			glBindBuffer(GL_ARRAY_BUFFER, 0); CHECKGL
			// Render quad
			glDrawArrays(GL_TRIANGLES, 0, 6); CHECKGL
			// Advance cursors for next glyph (note that advance is number of 1/64 pixels)
			x += (character.advance >> 6) * scale_x; // Bitshift by 6 to get value in pixels (2^6 = 64)
		}

		glBindVertexArray(0); CHECKGL
		glBindTexture(GL_TEXTURE_2D, 0); CHECKGL
	}

	float TextRenderer::textWidth(std::string_view text, float scale) {
		float out = 0.f;
		for (const char ch: text)
			out += scale * (characters.at(ch).advance >> 6);
		return out;
	}

	float TextRenderer::textHeight(std::string_view text, float scale) {
		float out = 0.f;
		for (const char ch: text)
			out = std::max(out, characters.at(ch).size.y * scale);
		return out;
	}

	void TextRenderer::setupShader(std::string_view text, const TextRenderOptions &options) {
		const auto text_width = textWidth(text, 1.f);
		const auto text_height = textHeight(text, 1.f);

		// const float text_width = 1.f / 20.f;
		// const float text_height = 1.f / 7.f;
		// const float text_width = 1.f;
		// const float text_height = 1.f;

		glm::mat4 model = glm::mat4(1.f);
		// first translate (transformations are: scale happens first, then rotation, and then final translation happens; reversed order)
		// model = glm::translate(model, glm::vec3(options.x + text_width / 2.f, options.y, 0.0f));
		// model = glm::translate(model, glm::vec3(options.x * 16.f, options.y * 16.f, 0.f));
		// model = glm::translate(model, glm::vec3(.5f * text_width, .5f * text_height, 0.0f));
		// model = glm::rotate   (model, glm::radians(options.angle), glm::vec3(0.0f, 0.0f, 1.0f));
		// model = glm::translate(model, glm::vec3(-.5f * text_width, -.5f * text_height, 0.0f));
		// model = glm::scale    (model, glm::vec3(text_width * options.scaleX, text_height * options.scaleY, 1.f));





		model = glm::translate(model, glm::vec3(options.x, options.y, 0.f));
		// model = glm::translate(model, glm::vec3(0.5f * text_width, 0.5f * text_height, 0.f)); // move origin of rotation to center of quad
		// model = glm::rotate(model, glm::radians(options.angle), glm::vec3(0.f, 0.f, 1.f)); // then rotate
		// model = glm::translate(model, glm::vec3(-0.5f * text_width, -0.5f * text_height, 0.f)); // move origin back
		model = glm::scale(model, glm::vec3(text_width * canvas->scale / 2.f, text_height * canvas->scale / 2.f, 2.f)); // last scale




		shader.bind();
		// shader.set("model", model);
		std::cout << glm::to_string(projection * v4) << '\n';
		// std::cout << "\e[35mText model: " << glm::to_string(model) << "\e[39m text<" << text_width << " x " << text_height << ">\n";
		shader.set("textColor", options.color.red, options.color.green, options.color.blue, options.color.alpha);
	}
}
