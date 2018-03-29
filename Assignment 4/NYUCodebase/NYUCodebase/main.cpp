#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <math.h>
#include <vector>
#include <iostream>
#include "Matrix.h"
#include "ShaderProgram.h"
#include "FlareMap.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define LEVEL_WIDTH 64
#define LEVEL_HEIGHT 48
#define SPRITE_COUNT_X 43
#define SPRITE_COUNT_Y 23
#define TILE_SIZE 0.2f
#define FRICTION_X 0.2f
#define FRICTION_Y 0.1f
#define GRAVITY 9.8f


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

void worldToTile(float worldX, float worldY, int* gridX, int* gridY) {
	*gridX = (int)(worldX / TILE_SIZE);
	*gridY = (int)(-worldY / TILE_SIZE);
}

float lerp(float v0, float v1, float t) {
	return (1.0f - t) * v0 + t * v1;
}

class Entity {
public:
	Entity() {}
	Entity(float x, float y, float z, float velocity_x, float velocity_y, float velocity_z, float accel_x, float accel_y, float accel_z, float size_x, float size_y, float size_z, const string& Type, const SheetSprite& mySprite) {
		position = Vector3(x, y, z);
		velocity = Vector3(velocity_x, velocity_y, velocity_z);
		acceleration = Vector3(accel_x, accel_y, accel_z);
		size = Vector3(size_x, size_y, size_z);
		type = Type;
		sprite = mySprite;
	}

	void draw(ShaderProgram* program, const Entity& player) {
		Matrix modelMatrix;
		Matrix projectionMatrix;
		Matrix viewMatrix;
		projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
		glUseProgram(program->programID);
		glBindTexture(GL_TEXTURE_2D, sprite.textureID);
		modelMatrix.Identity();
		modelMatrix.Translate(position.x, position.y, position.z);
		viewMatrix.Identity();
		viewMatrix.Translate(-player.position.x, -player.position.y, -player.position.z);
		program->SetModelMatrix(modelMatrix);
		program->SetProjectionMatrix(projectionMatrix);
		program->SetViewMatrix(viewMatrix);
		float vertices[] = { -0.5f * size.x, 0.5f * size.y, -0.5f * size.x, -0.5f * size.y, 0.5f * size.x, 0.5f * size.y,
							 0.5f * size.x, -0.5f * size.y, 0.5f * size.x, 0.5f * size.y, -0.5f * size.x, -0.5f * size.y };
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		float texCoords[] = { sprite.u, sprite.v, sprite.u, sprite.v + sprite.height, sprite.u + sprite.width, sprite.v,
							  sprite.u + sprite.width, sprite.v + sprite.height, sprite.u + sprite.width, sprite.v, sprite.u, sprite.v + sprite.height };
		glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program->texCoordAttribute);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program->positionAttribute);
		glDisableVertexAttribArray(program->texCoordAttribute);
	}

	void collideY(unsigned int** levelData) {
		int gridX, gridUpY, gridDownY;
		worldToTile(position.x, position.y + 0.5f * size.y, &gridX, &gridUpY);
		worldToTile(position.x, position.y - 0.5f * size.y, &gridX, &gridDownY);
		if (levelData[gridUpY][gridX] == 260 || levelData[gridUpY][gridX] == 691) {
			float penetration = position.y + 0.5f * size.y - (-TILE_SIZE * (gridUpY)-TILE_SIZE);
			position.y -= (penetration + 0.001f);
			velocity.y = 0.0f;
		}
		else if (levelData[gridDownY][gridX] == 260 || levelData[gridDownY][gridX] == 691) {
			float penetration = -TILE_SIZE * (gridDownY)-(position.y - 0.5f * size.y);
			position.y += (penetration + 0.001f);
			velocity.y = 0.0f;
			collideBot = true;
		}
	}

	void collideX(unsigned int** levelData) {
		int gridLeftX, gridRightX, gridY;
		worldToTile(position.x - 0.5f * size.x, position.y, &gridLeftX, &gridY);
		worldToTile(position.x + 0.5f * size.x, position.y, &gridRightX, &gridY);
		if (levelData[gridY][gridLeftX] == 694 || levelData[gridY][gridLeftX] == 691 || levelData[gridY][gridLeftX] == 260) {
			float penetration = TILE_SIZE * (gridLeftX)+TILE_SIZE - (position.x - 0.5f * size.x);
			position.x += (penetration + 0.001f);
			velocity.x = 0.0f;
			if (type == "enemy") {
				acceleration.x = -acceleration.x;
			}
		}
		else if (levelData[gridY][gridRightX] == 694 || levelData[gridY][gridRightX] == 691 || levelData[gridY][gridRightX] == 260) {
			float penetration = position.x + 0.5f * size.x - TILE_SIZE * (gridRightX);
			position.x -= (penetration + 0.001f);
			velocity.x = 0.0f;
			if (type == "enemy") {
				acceleration.x = -acceleration.x;
			}
		}
	}

	void update(float& elapsed, unsigned int** levelData) {
		if (type == "player") {
			velocity.x = lerp(velocity.x, 0.0f, elapsed * FRICTION_X);
			velocity.y = lerp(velocity.y, 0.0f, elapsed * FRICTION_Y);
			velocity.x += acceleration.x * elapsed;
			velocity.y += (acceleration.y - GRAVITY) * elapsed;
			position.y += elapsed * velocity.y;
			collideY(levelData);
			position.x += elapsed * velocity.x;
			collideX(levelData);
		}
		else if (type == "enemy"){
			velocity.x = lerp(velocity.x, 0.0f, elapsed * FRICTION_X);
			velocity.y = lerp(velocity.y, 0.0f, elapsed * FRICTION_Y);
			velocity.x += acceleration.x * elapsed;
			velocity.y += (acceleration.y - GRAVITY) * elapsed;
			position.y += elapsed * velocity.y;
			collideY(levelData);
			position.x += elapsed * velocity.x;
			collideX(levelData);
		}
	}

	bool collideEnemy(const Entity& enemy) {
		if (position.x + 0.5f * size.x < enemy.position.x - 0.5f * enemy.size.x ||
			position.x - 0.5f * size.x > enemy.position.x + 0.5f * enemy.size.x ||
			position.y + 0.5f * size.y < enemy.position.y - 0.5f * enemy.size.y ||
			position.y - 0.5f * size.y > enemy.position.y + 0.5f * enemy.size.y) {
			return false;
		}
		else {
			return true;
		}
	}

	Vector3 position;
	Vector3 velocity;
	Vector3 acceleration;
	Vector3 size;
	SheetSprite sprite;
	string type;
	bool collideBot = false;
};

void drawTile(ShaderProgram* program, GLuint& textureID, unsigned int** levelData, const Entity& player) {
	vector<float> vertexData;
	vector<float> texCoordData;
	int counter = 0;
	for (int y = 0; y < LEVEL_HEIGHT; y++) {
		for (int x = 0; x < LEVEL_WIDTH; x++) {
			if (levelData[y][x] != 0) {
				counter += 1;
				float u = (float)(((int)levelData[y][x] & SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
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
	Entity player;
	Entity enemy;
};

void placeEntity(GameState& state, const string& type, float position_x, float position_y, const SheetSprite& mySprite) {
	if (type == "player") {
		state.player = Entity(position_x, position_y, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0f, type, mySprite);
	}
	else if (type == "enemy") {
		state.enemy = Entity(position_x, position_y, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0f, type, mySprite);
	}
}

void setUp() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif
	glViewport(0, 0, 640, 360);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void processEvents(GameState& state, SDL_Event& event, bool& done, unsigned int** levelData) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
					if (state.player.collideBot) {
						state.player.velocity.y = 6.0f;
						state.player.collideBot = false;
					}
				}
			}
		}
		const Uint8* keys = SDL_GetKeyboardState(NULL);
		if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
			state.player.acceleration.x = -2.0f;
		}
		else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
			state.player.acceleration.x = 2.0f;
		}
}

void Update(GameState& state, bool& done, bool& flag, unsigned int** levelData, float& elapsed) {
	state.player.update(elapsed, levelData);
	state.enemy.update(elapsed, levelData);
	state.player.acceleration.x = 0.0f;
	if (state.player.collideEnemy(state.enemy)) {
		flag = true;
	}
}

void render(GameState& state, ShaderProgram* program, GLuint& textureID, unsigned int** levelData, bool& flag) {
	glClear(GL_COLOR_BUFFER_BIT);
	drawTile(program, textureID, levelData, state.player);
	state.player.draw(program, state.player);
	if (!flag) {
		state.enemy.draw(program, state.player);
	}
}

int main(int argc, char *argv[])
{
	setUp();
	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GameState state;
	GLuint textureID = loadTexture("platformer.png");
	unsigned int** levelData = new unsigned int*[LEVEL_HEIGHT];
	for (int i = 0; i < LEVEL_HEIGHT; i++) {
		levelData[i] = new unsigned int[LEVEL_WIDTH];
	}
	unsigned int source[LEVEL_HEIGHT][LEVEL_WIDTH] = 
	{
	{692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,261,261,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,261,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,261,261,261,0,0,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,0,0,0,0,0,0,0,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,261,0,0,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,261,261,261,261,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,261,261,261,261,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{692,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,692},
	{695,261,261,261,261,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,261,261,261,261,261,261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,261,261,261,261,0,0,0,0,261,261,261,261,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{695,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,695},
	{261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261,261}
	};
	for (int y = 0; y < LEVEL_HEIGHT; y++) {
		for (int x = 0; x < LEVEL_WIDTH; x++) {
			if (source[y][x] != 0) {
				levelData[y][x] = source[y][x] - 1;
			}
			else {
				levelData[y][x] = 0;
			}
		}
	}
	float player_u = (float)((793 % SPRITE_COUNT_X) / SPRITE_COUNT_X);
	float player_v = (float)((793 / SPRITE_COUNT_X) / SPRITE_COUNT_Y);
	SheetSprite playerSprite = SheetSprite(textureID, player_u, player_v, 1.0f / SPRITE_COUNT_X, 1.0f / SPRITE_COUNT_Y, TILE_SIZE);
	placeEntity(state, "player", 4 * TILE_SIZE + 0.5f * TILE_SIZE, -45 * TILE_SIZE - 0.5f * TILE_SIZE, playerSprite);
	float enemy_u = (float)((968 % SPRITE_COUNT_X) / SPRITE_COUNT_X);
	float enemy_v = (float)((968 / SPRITE_COUNT_X) / SPRITE_COUNT_Y);
	SheetSprite enemySprite = SheetSprite(textureID, enemy_u, enemy_v, 1.0f / SPRITE_COUNT_X, 1.0f / SPRITE_COUNT_Y, TILE_SIZE);
	placeEntity(state, "enemy", 18 * TILE_SIZE + 0.5F * TILE_SIZE, -39 * TILE_SIZE - 0.5F * TILE_SIZE, enemySprite);
	bool done = false;
	bool flag = false;
	SDL_Event event;
	float lastFrameTicks = 0.0f;
	while (!done) {
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		render(state, &program, textureID, levelData, flag);
		processEvents(state, event, done, levelData);
		Update(state, done, flag, levelData, elapsed); 
		SDL_GL_SwapWindow(displayWindow);
	}
	SDL_Quit();
	return 0;
}


