#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <math.h>
#include <vector>
#include <iostream>
#include <SDL_mixer.h>
#include "Matrix.h"
#include "ShaderProgram.h"
#include "FlareMap.h"
#include "SatCollision.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define LEVEL_WIDTH 64
#define LEVEL_HEIGHT 48
#define SPRITE_COUNT_X 43
#define SPRITE_COUNT_Y 23
#define TILE_SIZE 0.2f


#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif
using namespace std;

SDL_Window* displayWindow;

GLuint loadTexture(const char* filePath) {
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

void drawText(ShaderProgram* program, int fontTexture, const string& text, float size, float spacing, float start_x, float start_y) {
	float textureSize = 1 / 16.0f;
	vector<float> vertexData;
	vector<float> texData;
	for (size_t i = 0; i < text.size(); i++) {
		int spriteIndex = (int)text[i];
		float texture_X = (float)(spriteIndex % 16) / 16.0f;
		float texture_Y = (float)(spriteIndex / 16) / 16.0f;
		vertexData.insert(vertexData.end(), {
			((size + spacing) * i + (-0.5f * size)), 0.5f * size,
			((size + spacing) * i + (-0.5f * size)), -0.5f * size,
			((size + spacing) * i + (0.5f * size)), 0.5f * size,
			((size + spacing) * i + (0.5f * size)), -0.5f * size,
			((size + spacing) * i + (0.5f * size)), 0.5f * size,
			((size + spacing) * i + (-0.5f * size)), -0.5f * size });
		texData.insert(texData.end(), {
			texture_X, texture_Y,
			texture_X, texture_Y + textureSize,
			texture_X + textureSize, texture_Y,
			texture_X + textureSize, texture_Y + textureSize,
			texture_X + textureSize, texture_Y,
			texture_X, texture_Y + textureSize });
	}
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;
	projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
	glUseProgram(program->programID);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	modelMatrix.Identity();
	modelMatrix.Translate(start_x, start_y, 0.0f);
	program->SetModelMatrix(modelMatrix);
	program->SetProjectionMatrix(projectionMatrix);
	program->SetViewMatrix(viewMatrix);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

class Vector3 {
public:
	Vector3() {}
	Vector3(float input_x, float input_y, float input_z) {
		x = input_x;
		y = input_y;
		z = input_z;
	}
	float x;
	float y;
	float z;
};

class SheetSprite {
public:
	SheetSprite() {}
	SheetSprite(GLuint input_textureID, float input_u, float input_v, float input_width, float input_height, float input_size) {
		textureID = input_textureID;
		u = input_u;
		v = input_v;
		width = input_width;
		height = input_height;
		size = input_size;
	}
	GLuint textureID;
	float u;
	float v;
	float width;
	float height;
	float size;
};

class Entity {
public:
	Entity() {}
	Entity(float x, float y, float z, float velocity_x, float velocity_y, float velocity_z, float accel_x, float accel_y, float accel_z, float size_x, float size_y, float size_z, float angle, Entity* myParent) {
		position = Vector3(x, y, z);
		velocity = Vector3(velocity_x, velocity_y, velocity_z);
		acceleration = Vector3(accel_x, accel_y, accel_z);
		size = Vector3(size_x, size_y, size_z);
		parent = myParent;
		rotation = angle;
		points.push_back(Vector4(-0.5f * size_x, 0.5f * size_y, 0.0f));
		points.push_back(Vector4(0.5f * size_x, 0.5f * size_y, 0.0f));
		points.push_back(Vector4(0.5f * size_x, -0.5f * size_y, 0.0f));
		points.push_back(Vector4(-0.5f * size_x, -0.5f * size_y, 0.0f));
	}

	void draw(ShaderProgram* program) {
		Matrix modelMatrix;
		Matrix projectionMatrix;
		Matrix viewMatrix;
		projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
		glUseProgram(program->programID);
		modelMatrix.Identity();
		modelMatrix.Translate(position.x, position.y, position.z);
		modelMatrix.Rotate(rotation);
		modelMatrix.Scale(2.0f, 2.0f, 1.0f);
		Model = modelMatrix;
		if (parent) {
			modelMatrix = modelMatrix * parent->Model;
		}
		program->SetModelMatrix(modelMatrix);
		program->SetProjectionMatrix(projectionMatrix);
		program->SetViewMatrix(viewMatrix);
		float vertices[] = { -0.5f * size.x, 0.5f * size.y, -0.5f * size.x, -0.5f * size.y, 0.5f * size.x, 0.5f * size.y,
							 0.5f * size.x, -0.5f * size.y, 0.5f * size.x, 0.5f * size.y, -0.5f * size.x, -0.5f * size.y };
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program->positionAttribute);
	}
	void update(float elapsed) {
		position.x += velocity.x * elapsed;
		position.y += velocity.y * elapsed;
		position.z += velocity.z * elapsed;
		if (position.x >= 3.55f) {
			float penetration = position.x - 3.55f;
			position.x -= (penetration + 0.01f);
			velocity.x = -velocity.x;
		}
		else if (position.x <= -3.55f) {
			float penetration = -3.55f - position.x;
			position.x += (penetration + 0.01f);
			velocity.x = -velocity.x;
		}
		else if (position.y >= 2.0f) {
			float penetration = position.y - 2.0f;
			position.y -= (penetration + 0.01f);
			velocity.y = -velocity.y;
		}
		else if (position.y <= -2.0f) {
			float penetration = -2.0f - position.y;
			position.y += (penetration + 0.01f);
			velocity.y = -velocity.y;
		}
	}
	Vector3 position;
	Vector3 velocity;
	Vector3 acceleration;
	Vector3 size;
	Entity* parent;
	Matrix Model;
	vector<Vector4> points;
	float rotation;
};

void drawTile(ShaderProgram* program, GLuint& textureID, unsigned int** levelData, const Entity& player) {
	vector<float> vertexData;
	vector<float> texCoordData;
	int counter = 0;
	for (int y = 0; y < LEVEL_HEIGHT; y++) {
		for (int x = 0; x < LEVEL_WIDTH; x++) {
			if (levelData[y][x] != 0) {
				counter += 1;
				float u = (float)(((int)levelData[y][x] % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
				float v = (float)(((int)levelData[y][x] / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y);
				float spriteWidth = 1.0f / SPRITE_COUNT_X;
				float spriteHeight = 1.0f / SPRITE_COUNT_Y;
				vertexData.insert(vertexData.end(), {
					TILE_SIZE * x, -TILE_SIZE * y,
					TILE_SIZE * x, -TILE_SIZE * y - TILE_SIZE,
					TILE_SIZE * x + TILE_SIZE, -TILE_SIZE * y - TILE_SIZE,
					TILE_SIZE * x, -TILE_SIZE * y,
					TILE_SIZE * x + TILE_SIZE, -TILE_SIZE * y - TILE_SIZE,
					TILE_SIZE * x + TILE_SIZE, -TILE_SIZE * y });
				texCoordData.insert(texCoordData.end(), {
					u, v,
					u, v + spriteHeight,
					u + spriteWidth, v + spriteHeight,
					u, v,
					u + spriteWidth, v + spriteHeight,
					u + spriteWidth, v });
			}
		}
	}
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;
	projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
	glUseProgram(program->programID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	viewMatrix.Identity();
	viewMatrix.Translate(-player.position.x, -player.position.y, -player.position.z);
	program->SetModelMatrix(modelMatrix);
	program->SetProjectionMatrix(projectionMatrix);
	program->SetViewMatrix(viewMatrix);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	glDrawArrays(GL_TRIANGLES, 0, counter * 6);
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

class GameState {
public:
	GameState() {}
	Entity first;
	Entity second;
	Entity third;
};

void setUp() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
#ifdef _WINDOWS
	glewInit();
#endif
	glViewport(0, 0, 640, 360);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void processEvents(SDL_Event& event, bool& done) {
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
}

void Update(GameState& state, float& elapsed, Mix_Chunk* someSound) {
	state.first.update(elapsed);
	state.second.update(elapsed);
	state.third.update(elapsed);
	pair<float, float> penetration;
	vector<pair<float, float>> firstPoints;
	vector<pair<float, float>> secondPoints;
	vector<pair<float, float>> thirdPoints;
	for (size_t i = 0; i < state.first.points.size(); i++) {
		Vector4 pointFirst = state.first.Model * state.first.points[i];
		firstPoints.push_back(make_pair(pointFirst.x, pointFirst.y));
	}
	for (size_t j = 0; j < state.second.points.size(); j++) {
		Vector4 pointSecond = state.second.Model * state.second.points[j];
		secondPoints.push_back(make_pair(pointSecond.x, pointSecond.y));
	}
	for (size_t k = 0; k < state.third.points.size(); k++) {
		Vector4 pointThird = state.third.Model * state.third.points[k];
		thirdPoints.push_back(make_pair(pointThird.x, pointThird.y));
	}
	bool firColSec = CheckSATCollision(firstPoints, secondPoints, penetration);
	if (firColSec) {
		state.first.position.x += penetration.first * 0.5f;
		state.first.position.y += penetration.second * 0.5f;
		state.second.position.x -= penetration.first * 0.5f;
		state.second.position.y -= penetration.second * 0.5f;
		state.first.velocity.x = -state.first.velocity.x;
		state.first.velocity.y = -state.first.velocity.y;
		state.second.velocity.x = -state.second.velocity.x;
		state.second.velocity.y = -state.second.velocity.y;
		Mix_PlayChannel(-1, someSound, 0);
	}
	bool firColTrd = CheckSATCollision(firstPoints, thirdPoints, penetration);
	if (firColTrd) {
		state.first.position.x += penetration.first * 0.5f;
		state.first.position.y += penetration.second * 0.5f;
		state.third.position.x -= penetration.first * 0.5f;
		state.third.position.y -= penetration.second * 0.5f;
		state.first.velocity.x = -state.first.velocity.x;
		state.first.velocity.y = -state.first.velocity.y;
		state.third.velocity.x = -state.third.velocity.x;
		state.third.velocity.y = -state.third.velocity.y;
		Mix_PlayChannel(-1, someSound, 0);
	}
	bool secColTrd = CheckSATCollision(secondPoints, thirdPoints, penetration);
	if (secColTrd) {
		state.second.position.x += penetration.first * 0.5f;
		state.second.position.y += penetration.second * 0.5f;
		state.third.position.x -= penetration.first * 0.5f;
		state.third.position.y -= penetration.second * 0.5f;
		state.second.velocity.x = -state.second.velocity.x;
		state.second.velocity.y = -state.second.velocity.y;
		state.third.velocity.x = -state.third.velocity.x;
		state.third.velocity.y = -state.third.velocity.y;
		Mix_PlayChannel(-1, someSound, 0);
	}
}

void render(GameState& state, ShaderProgram* program) {
	glClear(GL_COLOR_BUFFER_BIT);
	state.first.draw(program);
	state.second.draw(program);
	state.third.draw(program);
}

int main(int argc, char *argv[])
{
	setUp();
	Mix_Chunk* someSound;
	someSound = Mix_LoadWAV("someSound.wav");
	Mix_Music *someMusic;
	someMusic = Mix_LoadMUS("someMusic.mp3");
	Mix_PlayMusic(someMusic, -1);
	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	GameState state;
	state.first = Entity(-2.0f, 1.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.6f, 0.0f, 45.0f * 3.1415926f / 180.0f, nullptr);
	state.second = Entity(2.0f, 1.5f, 0.0f, -1.5f, -1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.3f, 0.0f, 60.0f * 3.1415926f / 180.0f, nullptr);
	state.third = Entity(0.0f, -1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.4f, 0.0f, 30.0f * 3.1415927f / 180.0f, nullptr);
	bool done = false;
	SDL_Event event;
	float lastFrameTicks = 0.0f;
	while (!done) {
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		render(state, &program);
		processEvents(event, done);
		state.first.update(elapsed);
		state.third.update(elapsed);
		state.second.update(elapsed);
		Update(state, elapsed, someSound);
		SDL_GL_SwapWindow(displayWindow);
	}
	Mix_FreeChunk(someSound);
	Mix_FreeMusic(someMusic);
	SDL_Quit();
	return 0;
}


