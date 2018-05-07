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
#include <algorithm>
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
#define ENEMY_GAP 0.3f


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

enum EnemyState {IDLE, ALERT, FLEE};

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

	void sensePlayer(const Entity& player) {
		int gridX, gridY, playerX, playerY;
		worldToTile(position.x, position.y, &gridX, &gridY);
		worldToTile(player.position.x, player.position.y, &playerX, &playerY);
		float distance = fabs(gridX - playerX) + fabs(gridY - playerY);
		if (distance < 15.0f && distance > 5.0f){
			state = ALERT;
		}
		else if (distance <= 5.0f){
			accumalator = 0.0f;
			state = FLEE;
			if (playerX >= gridX && velocity.x > 0.0f) {
				velocity.x = -velocity.x;
			}
			else if (playerX < gridX && velocity.x < 0.0f) {
				velocity.x = -velocity.x;
			}
		}
		else {
			accumalator = 0.0f;
			state = IDLE;
		}
	}

	void senseEdge(const FlareMap& map) {
		vector<int> solidTiles = { 122, 332, 126, 127, 70, 152 };
		switch (state) {
		case IDLE:
		case ALERT:
			int gridLeftX, gridRightX, gridY;
			worldToTile(position.x + 0.5f * size.x + 0.1f, position.y - 0.5f * size.y - 0.1f, &gridRightX, &gridY);
			worldToTile(position.x - 0.5f * size.x - 0.1f, position.y - 0.5f * size.y - 0.1f, &gridLeftX, &gridY);
			if (find(solidTiles.begin(), solidTiles.end(), map.mapData[gridY][gridLeftX]) == solidTiles.end()) {
				velocity.x = -velocity.x;
			}
			else if (find(solidTiles.begin(), solidTiles.end(), map.mapData[gridY][gridRightX]) == solidTiles.end()) {
				velocity.x = -velocity.x;
			}
			break;
		case FLEE:
			int gridLeftX, gridRightX, gridY;
			worldToTile(position.x + 0.5f * size.x + 0.1f, position.y - 0.5f * size.y - 0.1f, &gridRightX, &gridY);
			worldToTile(position.x - 0.5f * size.x - 0.1f, position.y - 0.5f * size.y - 0.1f, &gridLeftX, &gridY);
			if (find(solidTiles.begin(), solidTiles.end(), map.mapData[gridY][gridLeftX]) == solidTiles.end()) {
				velocity.y = 6.0f;
			}
			else if (find(solidTiles.begin(), solidTiles.end(), map.mapData[gridY][gridRightX]) == solidTiles.end()) {
				velocity.y = 6.0f;
			}
			break;
		}
	}

	Entity shoot(const SheetSprite& bulletSprite) {
		if (velocity.x > 0) {
			Entity bullet = Entity(position.x, position.y, position.z, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0F, "bullet", bulletSprite);
			bullet.parent = this;
			return bullet;
		}
		else if (velocity.x < 0){
			Entity bullet = Entity(position.x, position.y, position.z, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0f, "bullet", bulletSprite);
			bullet.parent = this;
			return bullet;
		}
	}

	void collideY(const FlareMap& map) {
		vector<int> solidTiles = { 122, 332, 126, 127, 70, 152 };
		int gridX, gridUpY, gridDownY;
		worldToTile(position.x, position.y + 0.5f * size.y, &gridX, &gridUpY);
		worldToTile(position.x, position.y - 0.5f * size.y, &gridX, &gridDownY);
		if (find(solidTiles.begin(), solidTiles.end(), map.mapData[gridUpY][gridX]) != solidTiles.end()) {
			float penetration = position.y + 0.5f * size.y - (-TILE_SIZE * (gridUpY)-TILE_SIZE);
			position.y -= (penetration + 0.001f);
			velocity.y = 0.0f;
		}
		else if (find(solidTiles.begin(), solidTiles.end(), map.mapData[gridDownY][gridX]) != solidTiles.end()) {
			float penetration = -TILE_SIZE * (gridDownY)-(position.y - 0.5f * size.y);
			position.y += (penetration + 0.001f);
			velocity.y = 0.0f;
			collideBot = true;
		}
	}

	void collideX(const FlareMap& map) {
		int gridLeftX, gridRightX, gridY;
		worldToTile(position.x - 0.5f * size.x, position.y, &gridLeftX, &gridY);
		worldToTile(position.x + 0.5f * size.x, position.y, &gridRightX, &gridY);
		if (map.mapData[gridY][gridLeftX] == 694 || map.mapData[gridY][gridLeftX] == 691 || map.mapData[gridY][gridLeftX] == 260) {
			float penetration = TILE_SIZE * (gridLeftX)+TILE_SIZE - (position.x - 0.5f * size.x);
			position.x += (penetration + 0.001f);
			if (type == "enemy") {
				velocity.x = -velocity.x;
			}
			else if (type == "bullet") {
				collideSide = true;
			}
			else if (type == "player") {
				velocity.x = 0.0f;
			}
		}
		else if (map.mapData[gridY][gridRightX] == 694 || map.mapData[gridY][gridRightX] == 691 || map.mapData[gridY][gridRightX] == 260) {
			float penetration = position.x + 0.5f * size.x - TILE_SIZE * (gridRightX);
			position.x -= (penetration + 0.001f);
			if (type == "enemy") {
				velocity.x = -velocity.x;
			}
			else if (type == "bullet") {
				collideSide = true;
			}
			else if (type == "player") {
				velocity.x = 0.0f;
			}
		}
	}

	void update(float& elapsed, FlareMap& map, const Entity& player) {
		if (type == "player") {
			velocity.x = lerp(velocity.x, 0.0f, elapsed * FRICTION_X);
			velocity.y = lerp(velocity.y, 0.0f, elapsed * FRICTION_Y);
			velocity.x += acceleration.x * elapsed;
			velocity.y += (acceleration.y - GRAVITY) * elapsed;
			position.y += elapsed * velocity.y;
			collideY(map);
			position.x += elapsed * velocity.x;
			collideX(map);
		}
		else if (type == "enemy") {
			accumalator += elapsed;
			velocity.y = lerp(velocity.y, 0.0f, elapsed * FRICTION_Y);
			velocity.y += (acceleration.y - GRAVITY) * elapsed;
			position.y += elapsed * velocity.y;
			collideY(map);
			position.x += elapsed * velocity.x;
			collideX(map);
			sensePlayer(player);
			senseEdge(map);
		}
		else if (type == "bullet") {
			position.x += elapsed * velocity.x;
			collideX(map);
		}
	}

	bool collide(Entity& other) {
		if (position.x + 0.5f * size.x < other.position.x - 0.5f * other.size.x ||
			position.x - 0.5f * size.x > other.position.x + 0.5f * other.size.x ||
			position.y + 0.5f * size.y < other.position.y - 0.5f * other.size.y ||
			position.y - 0.5f * size.y > other.position.y + 0.5f * other.size.y) {
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
	Entity* parent = nullptr;
	SheetSprite sprite;
	string type;
	bool collideBot = false;
	bool collideSide = false;
	bool alive = true;
	EnemyState state = IDLE;
	float accumalator = 0.0f;
};

void drawTile(ShaderProgram* program, int textureID, const FlareMap& map, const Entity& player) {
	vector<float> vertexData;
	vector<float> texCoordData;
	int counter = 0;
	for (int y = 0; y < map.mapHeight; y++) {
		for (int x = 0; x < map.mapWidth; x++) {
			if (map.mapData[y][x] != 0) {
				counter += 1;
				float u = (float)(((int)map.mapData[y][x] % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
				float v = (float)(((int)map.mapData[y][x] / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y);
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

void drawMovement(ShaderProgram* program, int textureID, const Entity& player, int index, int spriteCountX, int spriteCountY) {
	float u = (float)(((int)index) % spriteCountX) / (float)spriteCountX;
	float v = (float)(((int)index) / spriteCountX) / (float)spriteCountY;
	float spriteWidth = 1.0 / (float)spriteCountX;
	float spriteHeight = 1.0 / (float)spriteCountY;
	float texCoords[] = {
		u, v + spriteHeight,
		u + spriteWidth, v,
		u, v,
		u + spriteWidth, v,
		u, v + spriteHeight,
		u + spriteWidth, v + spriteHeight
	};
	float vertices[] = { player.position.x + 0.5f * player.size.x, player.position.y + 0.5f * player.size.y, player.position.x - 0.5f * player.size.x, player.position.y + 0.5f * player.size.y, player.position.x - 0.5f * player.size.x, player.position.y - 0.5f * player.size.y,
						 player.position.x + 0.5f * player.size.x, player.position.y - 0.5f * player.size.y, player.position.x + 0.5f * player.size.x, player.position.y + 0.5f * player.size.y, player.position.x - 0.5f * player.size.x, player.position.y - 0.5f * player.size.y };
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
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program->texCoordAttribute);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

void renderPlayer(ShaderProgram* program, int textureID, const Entity& player, const int* animation, const int numFrames, float animationElapsed, float framesPerSecond, float& elapsed, int currentIndex) {
	animationElapsed += elapsed;
	if (animationElapsed > 1.0 / framesPerSecond) {
		currentIndex++;
		animationElapsed = 0.0;
		if (currentIndex > numFrames - 1) {
			currentIndex = 0;
		}
	}
	drawMovement(program, textureID, player, animation[currentIndex], 7, 3);
}

class GameState {
public:
	GameState() {}
	Entity player;
	vector<Entity> enemies;
	vector<Entity> bullets;
};

enum GameMode {STATE_MAIN_MENU, STATE_LEVEL_ONE, STATE_LEVEL_TWO, STATE_LEVEL_THREE, STATE_GAME_OVER};

void placeEntity(GameState& state, const string& type, float position_x, float position_y, const SheetSprite& mySprite) {
	state.enemies.push_back(Entity(position_x, position_y, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0f, type, mySprite));
}

bool shouldRemove(const Entity& bullet) {
	return bullet.collideSide;
}

bool shouldDie(const Entity& enemy) {
	return enemy.alive;
}

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
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void processEvents(GameState& state, GameMode& mode, SDL_Event& event, bool& done, Mix_Chunk* jumpSound, const SheetSprite& bulletSprite) {
	switch (mode) {
	case STATE_MAIN_MENU:
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN) {
				float unit_X = (((float)event.button.x / 640.0f) * 3.554f) - 1.777f;
				float unit_Y = (((float)(360.0f - event.button.y) / 360.0f) * 2.0f) - 1.0f;
				if (unit_X >= -1.4f && unit_X <= 1.4f && unit_Y >= -1.0f && unit_Y <= 0.0f) {
					mode = STATE_LEVEL_ONE;
				}
			}
		}
		break;
	case STATE_LEVEL_ONE:
	case STATE_LEVEL_TWO:
	case STATE_LEVEL_THREE:
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
					if (state.player.collideBot) {
						Mix_PlayChannel(-1, jumpSound, 0);
						state.player.velocity.y = 6.0f;
						state.player.collideBot = false;
					}
				}
			}
		}
		const Uint8* keys = SDL_GetKeyboardState(NULL);
		if (keys[SDL_SCANCODE_LEFT]) {
			state.player.acceleration.x = -2.0f;
		}
		else if (keys[SDL_SCANCODE_RIGHT]) {
			state.player.acceleration.x = 2.0f;
		}
		else if (keys[SDL_SCANCODE_A]) {
			state.bullets.push_back(state.player.shoot(bulletSprite));
		}
		break;
	case STATE_GAME_OVER:
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN) {
				float unit_X = (((float)event.button.x / 640.0f) * 3.554f) - 1.777f;
				float unit_Y = (((float)(360.0f - event.button.y) / 360.0f) * 2.0f) - 1.0f;
				if (unit_X >= -1.4f && unit_X <= 1.4f && unit_Y >= -1.0f && unit_Y <= 0.0f) {
					mode = STATE_LEVEL_ONE;
				}
			}
		}
		break;
	}
}

void Update(GameState& state, GameMode& mode, bool& flag, FlareMap& map, float& elapsed, Mix_Chunk* shootSound, Mix_Chunk* deadSound, const SheetSprite& bulletSprite) {
	switch (mode) {
	case STATE_MAIN_MENU:
		break;
	case STATE_LEVEL_ONE:
		state.enemies.erase(remove_if(state.enemies.begin(), state.enemies.end(), shouldDie), state.enemies.end());
		if (state.enemies.size() == 0) {
			state.bullets.clear();
			mode = STATE_LEVEL_TWO;
		}
		state.bullets.erase(remove_if(state.bullets.begin(), state.bullets.end(), shouldRemove), state.bullets.end());
		for (size_t i = 0; i < state.bullets.size(); i++) {
			if (state.player.collide(state.bullets[i])) {
				state.bullets[i].collideSide = true;
				if (state.bullets[i].parent->type == "enemy") {
					state.enemies.clear();
					state.bullets.clear();
					state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -45 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0F, "player", SheetSprite());
					Mix_PlayChannel(-1, deadSound, 0);
					mode = STATE_GAME_OVER;
				}
			}
			for (size_t j = 0; j < state.enemies.size(); j++) {
				if (state.enemies[j].collide(state.bullets[i])) {
					state.bullets[i].collideSide = true;
					if (state.bullets[i].parent->type == "player") {
						state.enemies[j].alive = false;
					}
				}
			}
			state.bullets[i].update(elapsed, map, state.player);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			state.enemies[i].update(elapsed, map, state.player);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			if (state.enemies[i].state == ALERT && state.enemies[i].accumalator >= elapsed) {
				state.bullets.push_back(state.enemies[i].shoot(bulletSprite));
				state.enemies[i].accumalator -= elapsed;
				Mix_PlayChannel(-1, shootSound, 0);
			}
		}
		state.player.update(elapsed, map, state.player);
		break;
	case STATE_LEVEL_TWO:
		state.enemies.erase(remove_if(state.enemies.begin(), state.enemies.end(), shouldDie), state.enemies.end());
		if (state.enemies.size() == 0) {
			state.bullets.clear();
			mode = STATE_LEVEL_THREE;
		}
		state.bullets.erase(remove_if(state.bullets.begin(), state.bullets.end(), shouldRemove), state.bullets.end());
		for (size_t i = 0; i < state.bullets.size(); i++) {
			if (state.player.collide(state.bullets[i])) {
				state.bullets[i].collideSide = true;
				if (state.bullets[i].parent->type == "enemy") {
					state.enemies.clear();
					state.bullets.clear();
					state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -45 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0F, "player", SheetSprite());
					Mix_PlayChannel(-1, deadSound, 0);
					mode = STATE_GAME_OVER;
				}
			}
			for (size_t j = 0; j < state.enemies.size(); j++) {
				if (state.enemies[j].collide(state.bullets[i])) {
					state.bullets[i].collideSide = true;
					if (state.bullets[i].parent->type == "player") {
						state.enemies[j].alive = false;
					}
				}
			}
			state.bullets[i].update(elapsed, map, state.player);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			state.enemies[i].update(elapsed, map, state.player);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			if (state.enemies[i].state == ALERT && state.enemies[i].accumalator >= elapsed) {
				state.bullets.push_back(state.enemies[i].shoot(bulletSprite));
				state.enemies[i].accumalator -= elapsed;
				Mix_PlayChannel(-1, shootSound, 0);
			}
		}
		state.player.update(elapsed, map, state.player);
		break;
	case STATE_LEVEL_THREE:
		state.enemies.erase(remove_if(state.enemies.begin(), state.enemies.end(), shouldDie), state.enemies.end());
		if (state.enemies.size() == 0) {
			state.bullets.clear();
			state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -45 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0F, "player", SheetSprite());
			flag = true;
			mode = STATE_GAME_OVER;
		}
		state.bullets.erase(remove_if(state.bullets.begin(), state.bullets.end(), shouldRemove), state.bullets.end());
		for (size_t i = 0; i < state.bullets.size(); i++) {
			if (state.player.collide(state.bullets[i])) {
				state.bullets[i].collideSide = true;
				if (state.bullets[i].parent->type == "enemy") {
					state.enemies.clear();
					state.bullets.clear();
					state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -45 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0F, "player", SheetSprite());
					Mix_PlayChannel(-1, deadSound, 0);
					mode = STATE_GAME_OVER;
				}
			}
			for (size_t j = 0; j < state.enemies.size(); j++) {
				if (state.enemies[j].collide(state.bullets[i])) {
					state.bullets[i].collideSide = true;
					if (state.bullets[i].parent->type == "player") {
						state.enemies[j].alive = false;
					}
				}
			}
			state.bullets[i].update(elapsed, map, state.player);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			state.enemies[i].update(elapsed, map, state.player);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			if (state.enemies[i].state == ALERT && state.enemies[i].accumalator >= elapsed) {
				state.bullets.push_back(state.enemies[i].shoot(bulletSprite));
				state.enemies[i].accumalator -= elapsed;
				Mix_PlayChannel(-1, shootSound, 0);
			}
		}
		state.player.update(elapsed, map, state.player);
		break;
	case STATE_GAME_OVER:
		break;
	}
}

void render(GameState& state, GameMode& mode, ShaderProgram* program, int textureID, int fontTexture, int playerTexture, const Entity& player, const int* runAnimation, const int* jumpAnimation, const int jumpFrames, const int walkFrames, float& walkElapsed, float& jumpElapsed, float framesPerSecond, int& walkIndex, int& jumpIndex, const FlareMap& map, bool& flag, float& elapsed) {
	switch (mode) {
	case STATE_MAIN_MENU:
		glClear(GL_COLOR_BUFFER_BIT);
		drawText(program, fontTexture, "Welcome to Platformer!", 0.2f, 0.1f, -3.45f, 1.0f);
		drawText(program, fontTexture, "PLAY", 0.5f, 0.08f, -0.8f, -0.5f);
		break;
	case STATE_LEVEL_ONE:
	case STATE_LEVEL_TWO:
	case STATE_LEVEL_THREE:
		glClear(GL_COLOR_BUFFER_BIT);
		drawTile(program, textureID, map, state.player);
		if (state.player.velocity.x != 0.0f && state.player.velocity.y <= 0.0f) {
			renderPlayer(program, playerTexture, player, runAnimation, walkFrames, walkElapsed, framesPerSecond, elapsed, walkIndex);
		}
		else if (state.player.velocity.y > 0.0f) {
			renderPlayer(program, playerTexture, player, jumpAnimation, jumpFrames, jumpElapsed, framesPerSecond, elapsed, jumpIndex);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			state.enemies[i].draw(program, state.player);
		}
		for (size_t j = 0; j < state.bullets.size(); j++) {
			state.bullets[j].draw(program, state.player);
		}
		break;
	case STATE_GAME_OVER:
		if (!flag) {
			drawText(program, fontTexture, "YOU LOSE!", 0.4f, 0.1f, -2.0f, 1.0f);
		}
		else {
			drawText(program, fontTexture, "Congratulations!", 0.4f, 0.0f, -3.0f, 1.0f);
		}
		drawText(program, fontTexture, "Play Again?", 0.5f, 0.08f, -0.8f, -0.5f);
		break;
	}
}

int main(int argc, char *argv[])
{
	setUp();
	Mix_Chunk* shootSound;
	shootSound = Mix_LoadWAV("shoot.wav");
	Mix_Chunk* jumpSound;
	jumpSound = Mix_LoadWAV("jump.wav");
	Mix_Chunk* deadSound;
	deadSound = Mix_LoadWAV("dead.wav");
	Mix_Music* levelOne;
	levelOne = Mix_LoadMUS("levelOne.mp3");
	Mix_Music* levelTwo;
	levelTwo = Mix_LoadMUS("levelTwo.mp3");
	Mix_Music* levelThree;
	levelThree = Mix_LoadMUS("levelThree.mp3");
	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GameState state;
	GameMode mode = STATE_MAIN_MENU;
	GLuint textureID = loadTexture("platformer.png");
	GLuint fontTexture = loadTexture("font1.png");
	GLuint playerTexture = loadTexture("playerSprite.png");
	FlareMap map;
	map.Load("levelOne.txt");
	state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -45 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0F, "player", SheetSprite());
	float enemy_u = (float)((968 % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
	float enemy_v = (float)((968 / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y);
	SheetSprite enemySprite = SheetSprite(textureID, enemy_u, enemy_v, 1.0f / SPRITE_COUNT_X, 1.0f / SPRITE_COUNT_Y, TILE_SIZE);
	SheetSprite bulletSprite;
	placeEntity(state, "enemy", 47 * TILE_SIZE + 0.5F * TILE_SIZE, -16 * TILE_SIZE - 0.5F * TILE_SIZE, enemySprite);
	bool done = false;
	bool flag = false;
	SDL_Event event;
	float lastFrameTicks = 0.0f;
	const int runAnimation[] = { 7, 8, 9, 10, 11 };
	const int jumpAnimation[] = { 13 };
	const int walkFrames = 5;
	const int jumpFrames = 1;
	float walkElapsed = 0.0f;
	float jumpElapsed = 0.0f;
	float framesPerSecond = 30.0f;
	int walkIndex = 0;
	int jumpIndex = 0;
	while (!done) {
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		render(state, mode, &program, textureID, fontTexture, playerTexture, state.player, runAnimation, jumpAnimation, jumpFrames, walkFrames, walkElapsed, jumpElapsed, framesPerSecond, walkIndex, jumpIndex, map, flag, elapsed);
		processEvents(state, mode, event, done, jumpSound, bulletSprite);
		Update(state, mode, flag, map, elapsed, shootSound, deadSound, bulletSprite);
		SDL_GL_SwapWindow(displayWindow);
	}
	Mix_FreeChunk(jumpSound);
	Mix_FreeChunk(shootSound);
	Mix_FreeChunk(deadSound);
	Mix_FreeMusic(levelOne);
	Mix_FreeMusic(levelTwo);
	Mix_FreeMusic(levelThree);
	SDL_Quit();
	return 0;
}


