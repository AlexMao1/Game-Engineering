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

class playerOne {
public:
	void draw(ShaderProgram& program) {
		program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

		Matrix projectionMatrix;
		Matrix modelMatrix;
		Matrix viewMatrix;

		projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);

		glUseProgram(program.programID);

		modelMatrix.Identity();
		modelMatrix.Translate(x, y, 0.0f);

		program.SetModelMatrix(modelMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);
		program.SetColor(0.0f, 0.0f, 0.0f, 1.0f);

		float vertices[] = { -2.5f, 0.5f, -2.6f, 0.5f, -2.6f, -0.5f, -2.6f, -0.5f, -2.5f, -0.5f, -2.5f, 0.5f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program.positionAttribute);
	}
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.1f;
	float height = 1.0f;
	float velocity_x = 2.0f;
	float velocity_y = 2.0f;
};

class playerTwo {
public:
	void draw(ShaderProgram& program) {
		program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

		Matrix projectionMatrix;
		Matrix modelMatrix;
		Matrix viewMatrix;

		projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);

		glUseProgram(program.programID);

		modelMatrix.Identity();
		modelMatrix.Translate(x, y, 0.0f);

		program.SetModelMatrix(modelMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);
		program.SetColor(0.0f, 0.0f, 0.0f, 1.0f);

		float vertices[] = { 2.6f, 0.5f, 2.5f, 0.5f, 2.5f, -0.5f, 2.5f, -0.5f, 2.6f, -0.5f, 2.6f, 0.5f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program.positionAttribute);
	}
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.1f;
	float height = 1.0f;
	float velocity_x = 2.0f;
	float velocity_y = 2.0f;
};

class Ball {
public:
	void draw(ShaderProgram& program){
		program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

		Matrix projectionMatrix;
		Matrix modelMatrix;
		Matrix viewMatrix;

		projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);

		glUseProgram(program.programID);

		modelMatrix.Identity();
		modelMatrix.Translate(x, y, 0.0f);

		program.SetModelMatrix(modelMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);
		program.SetColor(0.0f, 0.0f, 0.0f, 1.0f);

		float vertices[] = { 0.05f, 0.05f, -0.05f, 0.05f, -0.05f, -0.05f, -0.05f, -0.05f, 0.05f, -0.05f, 0.05f, 0.05f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program.positionAttribute);
	}
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.1f;
	float height = 0.1f;
	float velocity_x = 4.0f;
	float velocity_y = 4.0f;
	float direction_x = 0.5f;
	float direction_y = (float)sqrt(3) / 2;
};

void setUp() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif
	glViewport(0, 0, 640, 360);
	glClearColor(0.0f, 256.0f, 0.0f, 0.5f);
}

void processEvents(SDL_Event& event, bool& done, playerOne& first, playerTwo& second, float& elapsed) {
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
	const Uint8* keys = SDL_GetKeyboardState(NULL);
	if (keys[SDL_SCANCODE_W]) {
		first.y += 1.0f * first.velocity_y * elapsed;
	}
	else if (keys[SDL_SCANCODE_S]) {
		first.y += (-1.0f) * first.velocity_y * elapsed;
	}
	else if (keys[SDL_SCANCODE_UP]) {
		second.y += 1.0f * second.velocity_y * elapsed;
	}
	else if (keys[SDL_SCANCODE_DOWN]) {
		second.y += (-1.0f) * second.velocity_y * elapsed;
	}
}

void update(playerOne& first, playerTwo& second, Ball& pong, bool& done, float& elapsed) {
	if (0.05f + pong.x >= 3.55f) {
		cout << "Player One Wins!\n";
		done = true;
	}
	else if (-0.05f + pong.x <= -3.55f) {
		cout << "Player Two Wins!\n";
		done = true;
	}
	else if (-0.05f + pong.y <= 0.5f + first.y && 0.05f + pong.y >= -0.5f + first.y && -0.05f + pong.x <= -2.5f && 0.05f + pong.x >= -2.6f) {
		pong.direction_x = -pong.direction_x;
		pong.x += elapsed * pong.velocity_x * pong.direction_x;
		pong.y += elapsed * pong.velocity_y * pong.direction_y;
	}
	else if (-0.05f + pong.y <= 0.5f + second.y && 0.05f + pong.y >= -0.5f + second.y && -0.05f + pong.x <= 2.6f && 0.05f + pong.x >= 2.5f) {
		pong.direction_x = -pong.direction_x;
		pong.x += elapsed * pong.velocity_x * pong.direction_x;
		pong.y += elapsed * pong.velocity_y * pong.direction_y;
	}
	else if (0.05f + pong.y >= 2.0f) {
		pong.direction_y = -pong.direction_y;
		pong.x += elapsed * pong.velocity_x * pong.direction_x;
		pong.y += elapsed * pong.velocity_y * pong.direction_y;
	}
	else if (-0.05f + pong.y <= -2.0f) {
		pong.direction_y = -pong.direction_y;
		pong.x += elapsed * pong.velocity_x * pong.direction_x;
		pong.y += elapsed * pong.velocity_y * pong.direction_y;
	}
	else {
		pong.x += elapsed * pong.velocity_x * pong.direction_x;
		pong.y += elapsed * pong.velocity_y * pong.direction_y;
	}
}

void render(ShaderProgram& program, playerOne& first, playerTwo& second, Ball& pong) {
	glClear(GL_COLOR_BUFFER_BIT);
	first.draw(program);
	second.draw(program);
	pong.draw(program);
}

int main(int argc, char *argv[])
{
	setUp();
	ShaderProgram program;
	playerOne first;
	playerTwo second;
	Ball pong;
	SDL_Event event;
	bool done = false;
	float lastFrameTicks = 0.0f;
	while (!done) {
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		processEvents(event, done, first, second, elapsed);
		update(first, second, pong, done, elapsed);
		render(program, first, second, pong);
		SDL_GL_SwapWindow(displayWindow);
	}
	SDL_Quit();
	return 0;
}
