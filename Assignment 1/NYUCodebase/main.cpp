#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <math.h>
#include "Matrix.h"
#include "ShaderProgram.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif
using namespace std;

SDL_Window* displayWindow;

GLuint LoadTexture(const char* filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);

	if (image == NULL) {
		cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}

	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(image);
	return retTexture;
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

	glViewport(0, 0, 640, 360);

	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	projectionMatrix.SetOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	ShaderProgram program1;
	program1.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	GLuint grassTexture = LoadTexture(RESOURCE_FOLDER"grass.jpg");
	GLuint fireTexture = LoadTexture(RESOURCE_FOLDER"fire.png");
	GLuint sunTexture = LoadTexture(RESOURCE_FOLDER"sun.png");

	Matrix modelMatrix1;
	Matrix projectionMatrix1;
	Matrix viewMatrix1;

	projectionMatrix1.SetOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	float lastFrameTicks = 0.0f;
	float distance = 0.0f;

	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}

		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		glUseProgram(program.programID);
		glClear(GL_COLOR_BUFFER_BIT);

		program.SetModelMatrix(modelMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);

		float vertices[] = { 2.0f, 1.0f, -2.0f, 1.0f, -2.0f, -1.0f, -2.0f, -1.0f, 2.0f, -1.0f, 2.0f, 1.0f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program.positionAttribute);

		glUseProgram(program1.programID);

		modelMatrix1.Identity();
		program1.SetModelMatrix(modelMatrix1);
		program1.SetProjectionMatrix(projectionMatrix1);
		program1.SetViewMatrix(viewMatrix1);

		glBindTexture(GL_TEXTURE_2D, grassTexture);

		float vertices1[] = { 2.0f, -0.2f, -2.0f, -0.2f, -2.0f, -0.8f, -2.0f, -0.8f, 2.0f, -0.8f, 2.0f, -0.2f };
		glVertexAttribPointer(program1.positionAttribute, 2, GL_FLOAT, false, 0, vertices1);
		glEnableVertexAttribArray(program1.positionAttribute);

		float texCoords[] = { 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0 };
		glVertexAttribPointer(program1.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program1.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program1.positionAttribute);
		glDisableVertexAttribArray(program1.texCoordAttribute);

		modelMatrix1.Identity();
		program1.SetModelMatrix(modelMatrix1);

		glBindTexture(GL_TEXTURE_2D, fireTexture);

		float vertices2[] = {2.0f, 0.5f, -2.0f, 0.5f, -2.0f, -0.2f, -2.0f, -0.2f, 2.0f, -0.2f, 2.0f, 0.5f};
		glVertexAttribPointer(program1.positionAttribute, 2, GL_FLOAT, false, 0, vertices2);
		glEnableVertexAttribArray(program1.positionAttribute);

		float texCoords1[] = { 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0 };
		glVertexAttribPointer(program1.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords1);
		glEnableVertexAttribArray(program1.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program1.positionAttribute);
		glDisableVertexAttribArray(program1.texCoordAttribute);

		modelMatrix1.Identity();
		distance += elapsed * 1.0f;
		if (distance <= 3.5f) {
			modelMatrix1.Translate(distance, 0.0f, 0.0f);
		}
		else if (distance > 3.5f && distance < 7.0f){
			modelMatrix1.Translate(7.0f - distance, 0.0f, 0.0f);
		}
		else {
			distance -= 7.0f;
			modelMatrix1.Translate(distance, 0.0f, 0.0f);
		}

		program1.SetModelMatrix(modelMatrix1);
	
		glBindTexture(GL_TEXTURE_2D, sunTexture);

		float vertices3[] = { -1.5f, 1.0f, -2.0f, 1.0f, -2.0f, 0.5f, -2.0f, 0.5f, -1.5f, 0.5f, -1.5f, 1.0f };
		glVertexAttribPointer(program1.positionAttribute, 2, GL_FLOAT, false, 0, vertices3);
		glEnableVertexAttribArray(program1.positionAttribute);

		float texCoords2[] = { 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0 };
		glVertexAttribPointer(program1.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords2);
		glEnableVertexAttribArray(program1.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program1.positionAttribute);
		glDisableVertexAttribArray(program1.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
