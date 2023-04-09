#pragma once

#include <array>
#include <functional>
#include <numeric>

#include <GL/gl.h>

// #define CHECKGL do { if (auto err = glGetError()) { std::cerr << "\e[31mError at " << __FILE__ << ':' << __LINE__ << ": " << gluErrorString(err) << "\e[39m\n"; } } while(0);
#define CHECKGL

namespace GL {
	inline void useTextureInFB(GLuint texture) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0); CHECKGL
	}

	inline void bindFB(GLuint framebuffer) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); CHECKGL
	}

	inline void clear(float r, float g, float b, float a = 1.f) {
		glClearColor(r, g, b, a); CHECKGL
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); CHECKGL
	}

	inline GLint getFB() {
		GLint fb = -1;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb); CHECKGL
		if (fb == -1)
			throw std::runtime_error("Couldn't get current framebuffer binding");
		return fb;
	}

	inline GLuint makeFloatTexture(GLsizei width, GLsizei height, GLint filter = GL_LINEAR) {
		GLuint texture = -1;
		glGenTextures(1, &texture); CHECKGL
		if (texture == static_cast<GLuint>(-1))
			throw std::runtime_error("Couldn't generate float texture");
		glBindTexture(GL_TEXTURE_2D, texture); CHECKGL
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr); CHECKGL
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter); CHECKGL
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter); CHECKGL
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); CHECKGL
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); CHECKGL
		return texture;
	}

	inline void deleteTexture(GLuint texture) {
		if (texture != 0) {
			glDeleteTextures(1, &texture); CHECKGL
		}
	}

	inline GLuint makeFBO() {
		GLuint fb = -1;
		glGenFramebuffers(1, &fb); CHECKGL
		if (fb == static_cast<GLuint>(-1))
			throw std::runtime_error("Couldn't generate FBO");
		return fb;
	}

	template <size_t N>
	inline GLuint makeFloatVAO(GLuint vbo, const std::array<int, N> &sizes) {
		GLuint vao = -1;

		glGenVertexArrays(1, &vao); CHECKGL
		if (vao == static_cast<GLuint>(-1))
			throw std::runtime_error("Couldn't generate float VAO");

		glBindVertexArray(vao); CHECKGL
		glBindBuffer(GL_ARRAY_BUFFER, vbo); CHECKGL

		int offset = 0;
		const auto stride = static_cast<GLsizei>(sizeof(float) * std::accumulate(sizes.begin(), sizes.end(), 0));

		for (size_t i = 0; i < N; ++i)  {
			glEnableVertexAttribArray(i); CHECKGL
			glVertexAttribPointer(i, N, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<GLvoid *>(sizeof(float) * offset)); CHECKGL
			offset += sizes[i];
		}

		return vao;
	}

	template <typename T>
	inline GLuint makeBufferObject(GLenum target, const T *data, size_t count, GLenum usage)  {
		GLuint bo = -1;
		glGenBuffers(1, &bo); CHECKGL
		if (bo == static_cast<GLuint>(-1))
			throw std::runtime_error("Couldn't generate buffer object");
		glBindBuffer(target, bo); CHECKGL
		glBufferData(target, count * sizeof(T), data, usage); CHECKGL
		return bo;
	}

	template <typename T>
	inline GLuint makeEBO(const T *data, size_t count, GLenum usage) {
		return makeBufferObject(GL_ELEMENT_ARRAY_BUFFER, data, count, usage);
	}

	template <typename T>
	inline GLuint makeVBO(const T *data, size_t count, GLenum usage) {
		return makeBufferObject(GL_ARRAY_BUFFER, data, count, usage);
	}

	template <typename T, size_t N>
	inline GLuint makeSquareVBO(size_t first, size_t second, GLenum usage, const std::function<std::array<std::array<T, N>, 4>(size_t, size_t)> &fn) {
		std::vector<T> vertex_data;
		vertex_data.reserve(first * second * (2 + N));

		for (size_t i = 0; i < first; ++i) {
			for (size_t j = 0; j < second; ++j) {
				const auto generated = fn(i, j);

				vertex_data.push_back(i);
				vertex_data.push_back(j);
				for (const auto &item: generated[0])
					vertex_data.push_back(item);

				vertex_data.push_back(i + 1);
				vertex_data.push_back(j);
				for (const auto &item: generated[1])
					vertex_data.push_back(item);

				vertex_data.push_back(i);
				vertex_data.push_back(j + 1);
				for (const auto &item: generated[2])
					vertex_data.push_back(item);

				vertex_data.push_back(i + 1);
				vertex_data.push_back(j + 1);
				for (const auto &item: generated[3])
					vertex_data.push_back(item);
			}
		}

		return GL::makeVBO(vertex_data.data(), vertex_data.size(), usage);
	}

	struct Viewport {
		GLint saved[4];

		Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
			glGetIntegerv(GL_VIEWPORT, saved); CHECKGL
			glViewport(x, y, width, height); CHECKGL
		}

		inline void reset() {
			glViewport(saved[0], saved[1], static_cast<GLsizei>(saved[2]), static_cast<GLsizei>(saved[3])); CHECKGL
		}
	};
}
