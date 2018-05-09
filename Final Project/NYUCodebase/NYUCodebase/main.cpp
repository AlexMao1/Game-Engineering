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
#define SPRITE_COUNT_X 30
#define SPRITE_COUNT_Y 16
#define TILE_SIZE 0.25f
#define FRICTION_X 0.2f
#define FRICTION_Y 0.1f
#define GRAVITY 9.8f
#define ENEMY_GAP 1.0f


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

void loadLevel(int**& mapData, int source[LEVEL_HEIGHT][LEVEL_WIDTH]) {
	for (size_t i = 0; i < LEVEL_HEIGHT; i++) {
		for (size_t j = 0; j < LEVEL_WIDTH; j++) {
			int val = source[i][j];
			if (val > 0) {
				mapData[i][j] = val - 1;
			}
			else {
				mapData[i][j] = 360;
			}
		}
	}
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

enum GameMode { STATE_MAIN_MENU, STATE_GUIDE_PAGE, STATE_LEVEL_ONE, STATE_LEVEL_TWO, STATE_LEVEL_THREE, STATE_GAME_OVER };

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
		float distance = float(fabs(gridX - playerX) + fabs(gridY - playerY));
		if (distance < 15.0f && distance > 5.0f){
			state = ALERT;
		}
		else if (distance <= 5.0f){
			accumalator = 0.0f;
			state = FLEE;
			if (playerX >= gridX && velocity.x > 0.0f) {
				velocity.x = -1.0f;
			}
			else if (playerX < gridX && velocity.x < 0.0f) {
				velocity.x = 1.0f;
			}
		}
		else {
			accumalator = 0.0f;
			state = IDLE;
		}
	}

	void senseEdge(int**& mapData) {
		vector<int> solidTiles = { 122, 332, 126, 127, 152, 395, 396, 397, 398, 252};
		int gridLeftX, gridRightX, gridY;
		worldToTile(position.x + 0.5f * size.x + 0.01f, position.y - 0.5f * size.y - 0.01f, &gridRightX, &gridY);
		worldToTile(position.x - 0.5f * size.x - 0.01f, position.y - 0.5f * size.y - 0.01f, &gridLeftX, &gridY);
		switch (state) {
		case IDLE:
		case ALERT:
			if ((find(solidTiles.begin(), solidTiles.end(), mapData[gridY][gridLeftX]) == solidTiles.end()) && collideBot) {
				velocity.x = -velocity.x;
			}
			else if ((find(solidTiles.begin(), solidTiles.end(), mapData[gridY][gridRightX]) == solidTiles.end()) && collideBot) {
				velocity.x = -velocity.x;
			}
			break;
		case FLEE:
			if ((find(solidTiles.begin(), solidTiles.end(), mapData[gridY][gridLeftX]) == solidTiles.end()) && collideBot) {
				velocity.y = 6.0f;
				collideBot = false;
			}
			else if ((find(solidTiles.begin(), solidTiles.end(), mapData[gridY][gridRightX]) == solidTiles.end()) && collideBot) {
				velocity.y = 6.0f;
				collideBot = false;
			}
			break;
		}
	}

	Entity shoot(const SheetSprite& bulletSprite) {
		if (velocity.x > 0) {
			Entity bullet = Entity(position.x + 0.5f * TILE_SIZE, position.y, position.z, 1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0f, "bullet", bulletSprite);
			bullet.parent = this;
			return bullet;
		}
		else if (velocity.x < 0){
			Entity bullet = Entity(position.x - 0.5f * TILE_SIZE, position.y, position.z, -1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0f, "bullet", bulletSprite);
			bullet.parent = this;
			return bullet;
		}
	}

	void collideY(GameMode& mode, int**& mapData) {
		vector<int> solidTiles = { 122, 332, 126, 127, 152, 395, 396, 397, 398, 252 };
		int gridX, gridUpY, gridDownY;
		worldToTile(position.x, position.y + 0.5f * size.y, &gridX, &gridUpY);
		worldToTile(position.x, position.y - 0.5f * size.y, &gridX, &gridDownY);
		if (find(solidTiles.begin(), solidTiles.end(), mapData[gridUpY][gridX]) != solidTiles.end()) {
			float penetration = position.y + 0.5f * size.y - (-TILE_SIZE * (gridUpY)-TILE_SIZE);
			position.y -= (penetration + 0.001f);
			velocity.y = 0.0f;
		}
		else if (find(solidTiles.begin(), solidTiles.end(), mapData[gridDownY][gridX]) != solidTiles.end()) {
			float penetration = -TILE_SIZE * (gridDownY)-(position.y - 0.5f * size.y);
			position.y += (penetration + 0.001f);
			velocity.y = 0.0f;
			collideBot = true;
		}
		else if (mapData[gridDownY][gridX] == 70) {
			float penetration = -TILE_SIZE * (gridDownY)-(position.y - 0.5f * size.y);
			position.y += (penetration + 0.001f);
			velocity.y = 0.0f;
			alive = false;
		}
		else if (mapData[gridDownY][gridX] == 130) {
			if (type == "player") {
				mapData[gridDownY][gridX] = 360;
				canShoot = true;
			}
		}
		else if (mapData[gridDownY][gridX] == 284) {
			if (type == "player") {
				velocity.y = 9.0f;
			}
		}
		else if (mapData[gridDownY][gridX] == 14) {
			if (type == "player") {
				switch (mode) {
				case STATE_LEVEL_TWO:
					mapData[gridDownY][gridX] = 360;
					mapData[27][62] = 360;
					break;
				case STATE_LEVEL_THREE:
					mapData[gridDownY][gridX] = 360;
					mapData[5][60] = 360;
					break;
				}
			}
		}
		else if (mapData[gridDownY][gridX] == 310) {
			if (type == "player") {
				exit = true;
			}
		}
	}

	void collideX(GameMode& mode, int**& mapData) {
		vector<int> solidTiles = { 122, 332, 126, 127, 152, 395, 396, 397, 398, 252, 284};
		int gridLeftX, gridRightX, gridY;
		worldToTile(position.x - 0.5f * size.x, position.y, &gridLeftX, &gridY);
		worldToTile(position.x + 0.5f * size.x, position.y, &gridRightX, &gridY);
		if (find(solidTiles.begin(), solidTiles.end(), mapData[gridY][gridLeftX]) != solidTiles.end()) {
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
		else if (find(solidTiles.begin(), solidTiles.end(), mapData[gridY][gridRightX]) != solidTiles.end()) {
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
		else if (mapData[gridY][gridLeftX] == 70 || mapData[gridY][gridRightX] == 70) {
			if (type == "player") {
				alive = false;
			}
			else if (type == "enemy") {
				if (mapData[gridY][gridLeftX] == 70) {
					float penetration = TILE_SIZE * (gridLeftX)+TILE_SIZE - (position.x - 0.5f * size.x);
					position.x += (penetration + 0.001f);
					velocity.x = -velocity.x;
				}
				else {
					float penetration = position.x + 0.5f * size.x - TILE_SIZE * (gridRightX);
					position.x -= (penetration + 0.001f);
					velocity.x = -velocity.x;
				}
			}
		}
		else if (mapData[gridY][gridLeftX] == 130) {
			if (type == "player") {
				mapData[gridY][gridLeftX] = 360;
				canShoot = true;
			}
		}
		else if (mapData[gridY][gridRightX] == 130) {
			if (type == "player") {
				mapData[gridY][gridRightX] = 360;
				canShoot = true;
			}
		}
		else if (mapData[gridY][gridLeftX] == 310 || mapData[gridY][gridRightX] == 310){
			exit = true;
		}
		else if (mapData[gridY][gridLeftX] == 14 || mapData[gridY][gridRightX] == 14){
			if (type == "player") {
				switch (mode) {
				case STATE_LEVEL_TWO:
					mapData[gridY][gridRightX] = 360;
					mapData[gridY][gridRightX + 1] = 360;
					mapData[27][62] = 360;
					break;
				case STATE_LEVEL_THREE:
					mapData[gridY][gridLeftX] = 360;
					mapData[5][60] = 360;
					break;
				}
			}
		}
	}

	void standOnBoard(const Entity& board) {
		if (type == "player" || type == "enemy") {
			if (position.x + 0.5f * size.x < board.position.x - 0.5f * board.size.x || position.x - 0.5f * size.x > board.position.x + 0.5f * board.size.x ||
				position.y - 0.5f * size.y > board.position.y + 0.5f * board.size.y || position.y + 0.5f * size.y < board.position.y - 0.5f * board.size.y) {
				onBoard = false;
			}
			else{
				if (position.y - 0.5f * size.y > board.position.y - 0.5f * board.size.y) {
					float penetration = board.position.y + 0.5f * board.size.y - (position.y - 0.5f * size.y);
					position.y += (penetration + 0.001f);
					velocity.y = 0.0f;
					collideBot = true;
					onBoard = true;
				}
				else {
					float penetration = position.y + 0.5f * size.y - (board.position.y - 0.5f * board.size.y);
					position.y -= (penetration + 0.001f);
					velocity.y = 0.0f;
					onBoard = false;
				}
			}
		}
	}

	void update(GameMode& mode, float& elapsed, int**& mapData, const Entity& player, const Entity& board) {
		if (type == "player") {
			if (onBoard) {
				velocity.x = board.velocity.x;
				velocity.x += float(acceleration.x / 2.0f);
			}
			else {
				velocity.x = lerp(velocity.x, 0.0f, elapsed * FRICTION_X);
				velocity.x += acceleration.x * elapsed;
			}
			velocity.y = lerp(velocity.y, 0.0f, elapsed * FRICTION_Y);
			velocity.y += (acceleration.y - GRAVITY) * elapsed;
			position.y += elapsed * velocity.y;
			standOnBoard(board);
			collideY(mode, mapData);
			position.x += elapsed * velocity.x;
			collideX(mode, mapData);
		}
		else if (type == "enemy") {
			accumalator += elapsed;
			velocity.y = lerp(velocity.y, 0.0f, elapsed * FRICTION_Y);
			velocity.y += (acceleration.y - GRAVITY) * elapsed;
			position.y += elapsed * velocity.y;
			collideY(mode, mapData);
			position.x += elapsed * velocity.x;
			collideX(mode, mapData);
			sensePlayer(player);
			senseEdge(mapData);
		}
		else if (type == "bullet") {
			position.x += elapsed * velocity.x;
			collideX(mode, mapData);
		}
		else if (type == "board") {
			position.x += elapsed * velocity.x;
			if (position.x > 53 * TILE_SIZE + 0.5f * TILE_SIZE) {
				velocity.x = -2.0f;
			}
			else if (position.x < 7 * TILE_SIZE + 0.5f * TILE_SIZE) {
				velocity.x = 2.0f;
			}
		}
	}

	bool switchOn(int**& mapData) {
		int gridX, gridDownY, gridLeftX, gridY;
		worldToTile(position.x, position.y - 0.5f * size.y, &gridX, &gridDownY);
		worldToTile(position.x - 0.5f * size.x, position.y, &gridLeftX, &gridY);
		if (mapData[gridY][gridLeftX] == 250) {
			float penetration = (TILE_SIZE * (gridLeftX)+TILE_SIZE) - (position.x - 0.5f * size.x);
			position.x += (penetration + 0.001f);
			velocity.x = 0.0f;
			return true;
		}
		else if (mapData[gridDownY][gridX] == 250) {
			float penetration = -TILE_SIZE * (gridDownY)-(position.y - 0.5f * size.y);
			position.y += (penetration + 0.001f);
			velocity.y = 0.0f;
			return true;
		}
		return false;
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
	bool canShoot = false;
	bool onBoard = false;
	bool exit = false;
	EnemyState state = IDLE;
	float accumalator = 0.0f;
};

void drawTile(ShaderProgram* program, int textureID, const FlareMap& map, const Entity& player, int**& mapData) {
	vector<float> vertexData;
	vector<float> texCoordData;
	int counter = 0;
	for (size_t y = 0; y < LEVEL_HEIGHT; y++) {
		for (size_t x = 0; x < LEVEL_WIDTH; x++) {
			if (mapData[y][x] != 0) {
				counter += 1;
				float u = (float)(((int)mapData[y][x] % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
				float v = (float)(((int)mapData[y][x] / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y);
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
	float spriteWidth = 1.0f / (float)spriteCountX;
	float spriteHeight = 1.0f / (float)spriteCountY;
	float texCoords[] = {
		u, v + spriteHeight,
		u + spriteWidth, v,
		u, v,
		u + spriteWidth, v,
		u, v + spriteHeight,
		u + spriteWidth, v + spriteHeight
	};
	float vertices[] = { -0.5f * player.size.x, -0.5f * player.size.y, 0.5f * player.size.x, 0.5f * player.size.y, -0.5f * player.size.x, 0.5f * player.size.y,
						 0.5f * player.size.x, 0.5f * player.size.y, -0.5f * player.size.x, -0.5f * player.size.y, 0.5f * player.size.x, -0.5f * player.size.y };
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;
	projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
	glUseProgram(program->programID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	modelMatrix.Identity();
	modelMatrix.Translate(player.position.x, player.position.y, player.position.z);
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

void renderPlayer(ShaderProgram* program, int textureID, const Entity& player, const int* animation, const int numFrames, float& animationElapsed, float framesPerSecond, float& elapsed, int& currentIndex) {
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
	Entity board;
};

void placeEntity(GameState& state, const string& type, float x, float y, const SheetSprite& mySprite) {
	state.enemies.push_back(Entity(x * TILE_SIZE + 0.5f * TILE_SIZE, -y * TILE_SIZE - 0.5f * TILE_SIZE, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE, TILE_SIZE, 0.0f, type, mySprite));
}

bool shouldRemove(const Entity& bullet) {
	return bullet.collideSide;
}

bool shouldDie(const Entity& enemy) {
	return !enemy.alive;
}

void setUp() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My World", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
#ifdef _WINDOWS
	glewInit();
#endif
	glViewport(0, 0, 1280, 720);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void processEvents(GameState& state, GameMode& mode, FlareMap& map, SDL_Event& event, bool& done, Mix_Chunk* jumpSound,const SheetSprite& playerSprite, const SheetSprite& enemySprite, const SheetSprite& bulletSprite, int levelOne[LEVEL_HEIGHT][LEVEL_WIDTH], int**& mapData) {
	const Uint8* keys = SDL_GetKeyboardState(NULL);
	switch (mode) {
	case STATE_MAIN_MENU:
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN) {
				mode = STATE_GUIDE_PAGE;
				break;
			}
		}
		break;
	case STATE_GUIDE_PAGE:
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN) {
				mode = STATE_LEVEL_ONE;
				break;
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
		if (keys[SDL_SCANCODE_LEFT]) {
			state.player.acceleration.x = -2.0f;
		}
		else if (keys[SDL_SCANCODE_RIGHT]) {
			state.player.acceleration.x = 2.0f;

		}
		else if (keys[SDL_SCANCODE_A]) {
			if (state.player.canShoot) {
				state.bullets.push_back(state.player.shoot(bulletSprite));
			}
		}
		else if (keys[SDL_SCANCODE_Q]) {
			state.board = Entity();
			state.enemies.clear();
			state.bullets.clear();
			state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -45 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE * 0.75f, TILE_SIZE, 0.0F, "player", playerSprite);
			map.Load("levelOne.txt");
			for (size_t i = 0; i < map.entities.size(); i++) {
				placeEntity(state, map.entities[i].type, map.entities[i].x, map.entities[i].y, enemySprite);
			}
			loadLevel(mapData, levelOne);
			mode = STATE_MAIN_MENU;
			break;
		}
		break;
	case STATE_GAME_OVER:
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN) {
				map.Load("levelOne.txt");
				state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -45 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE * 0.75f, TILE_SIZE, 0.0F, "player", playerSprite);
				for (size_t i = 0; i < map.entities.size(); i++) {
					placeEntity(state, map.entities[i].type, map.entities[i].x, map.entities[i].y, enemySprite);
				}
				loadLevel(mapData, levelOne);
				mode = STATE_LEVEL_ONE;
				break;
			}
		}
		break;
	}
}

void Update(GameState& state, GameMode& mode, bool& flag, FlareMap& map, float& elapsed, Mix_Chunk* shootSound, Mix_Chunk* deadSound, const SheetSprite& playerSprite, const SheetSprite& enemySprite, const SheetSprite& bulletSprite, const SheetSprite& boardSprite, int levelTwo[LEVEL_HEIGHT][LEVEL_WIDTH], int levelThree[LEVEL_HEIGHT][LEVEL_WIDTH], int**& mapData) {
	switch (mode) {
	case STATE_MAIN_MENU:
	case STATE_GUIDE_PAGE:
		break;
	case STATE_LEVEL_ONE:
		state.player.update(mode, elapsed, mapData, state.player, state.board);
		state.player.acceleration.x = 0.0f;
		if (!state.player.alive) {
			state.enemies.clear();
			state.bullets.clear();
			Mix_PlayChannel(-1, deadSound, 0);
			flag = false;
			mode = STATE_GAME_OVER;
			break;
		}
		state.enemies.erase(remove_if(state.enemies.begin(), state.enemies.end(), shouldDie), state.enemies.end());
		if (state.player.exit) {
			state.enemies.clear();
			state.bullets.clear();
			map.Load("levelTwo.txt");
			for (size_t i = 0; i < map.entities.size(); i++) {
				placeEntity(state, map.entities[i].type, map.entities[i].x, map.entities[i].y, enemySprite);
			}
			loadLevel(mapData, levelTwo);
			state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -46 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE * 0.75f, TILE_SIZE, 0.0F, "player", playerSprite);
			mode = STATE_LEVEL_TWO;
			break;
		}
		state.bullets.erase(remove_if(state.bullets.begin(), state.bullets.end(), shouldRemove), state.bullets.end());
		for (size_t i = 0; i < state.bullets.size(); i++) {
			if (state.player.collide(state.bullets[i])) {
				if (state.bullets[i].parent->type == "enemy") {
					state.enemies.clear();
					state.bullets.clear();
					Mix_PlayChannel(-1, deadSound, 0);
					flag = false;
					mode = STATE_GAME_OVER;
					break;
				}
			}
			for (size_t j = 0; j < state.enemies.size(); j++) {
				if (state.enemies[j].collide(state.bullets[i])) {
					if (state.bullets[i].parent->type == "player") {
						state.bullets[i].collideSide = true;
						state.enemies[j].alive = false;
					}
				}
			}
			state.bullets[i].update(mode, elapsed, mapData, state.player, state.board);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			if (state.enemies[i].collide(state.player)) {
				state.enemies.clear();
				state.bullets.clear();
				Mix_PlayChannel(-1, deadSound, 0);
				flag = false;
				mode = STATE_GAME_OVER;
				break;
			}
			if (state.enemies[i].state == ALERT && state.enemies[i].accumalator >= ENEMY_GAP) {
				state.bullets.push_back(state.enemies[i].shoot(bulletSprite));
				state.enemies[i].accumalator -= ENEMY_GAP;
				Mix_PlayChannel(-1, shootSound, 0);
			}
			state.enemies[i].update(mode, elapsed, mapData, state.player, state.board);
		}
		break;
	case STATE_LEVEL_TWO:
		state.player.update(mode, elapsed, mapData, state.player, state.board);
		state.player.acceleration.x = 0.0f;
		if (!state.player.alive) {
			state.enemies.clear();
			state.bullets.clear();
			Mix_PlayChannel(-1, deadSound, 0);
			flag = false;
			mode = STATE_GAME_OVER;
			break;
		}
		state.enemies.erase(remove_if(state.enemies.begin(), state.enemies.end(), shouldDie), state.enemies.end());
		if (state.player.exit) {
			state.enemies.clear();
			state.bullets.clear();
			map.Load("levelThree.txt");
			for (size_t i = 0; i < map.entities.size(); i++) {
				placeEntity(state, map.entities[i].type, map.entities[i].x, map.entities[i].y, enemySprite);
			}
			loadLevel(mapData, levelThree);
			state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -46 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE * 0.75f, TILE_SIZE, 0.0F, "player", playerSprite);
			state.board = Entity(7 * TILE_SIZE + 0.5f * TILE_SIZE, -41 * TILE_SIZE - 0.5f * TILE_SIZE, 0.0F, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4 * TILE_SIZE, TILE_SIZE, 0.0f, "board", boardSprite);
			mode = STATE_LEVEL_THREE;
			break;
		}
		state.bullets.erase(remove_if(state.bullets.begin(), state.bullets.end(), shouldRemove), state.bullets.end());
		for (size_t i = 0; i < state.bullets.size(); i++) {
			if (state.player.collide(state.bullets[i])) {
				if (state.bullets[i].parent->type == "enemy") {
					state.enemies.clear();
					state.bullets.clear();
					Mix_PlayChannel(-1, deadSound, 0);
					flag = false;
					mode = STATE_GAME_OVER;
					break;
				}
			}
			for (size_t j = 0; j < state.enemies.size(); j++) {
				if (state.enemies[j].collide(state.bullets[i])) {
					if (state.bullets[i].parent->type == "player") {
						state.bullets[i].collideSide = true;
						state.enemies[j].alive = false;
					}
				}
			}
			state.bullets[i].update(mode, elapsed, mapData, state.player, state.board);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			if (state.enemies[i].collide(state.player)) {
				state.enemies.clear();
				state.bullets.clear();
				Mix_PlayChannel(-1, deadSound, 0);
				flag = false;
				mode = STATE_GAME_OVER;
				break;
			}
			if (state.enemies[i].state == ALERT && state.enemies[i].accumalator >= ENEMY_GAP) {
				state.bullets.push_back(state.enemies[i].shoot(bulletSprite));
				state.enemies[i].accumalator -= ENEMY_GAP;
				Mix_PlayChannel(-1, shootSound, 0);
			}
			state.enemies[i].update(mode, elapsed, mapData, state.player, state.board);
		}
		break;
	case STATE_LEVEL_THREE:
		state.player.update(mode, elapsed, mapData, state.player, state.board);
		state.player.acceleration.x = 0.0f;
		if (!state.player.alive) {
			state.board = Entity();
			state.enemies.clear();
			state.bullets.clear();
			Mix_PlayChannel(-1, deadSound, 0);
			flag = false;
			mode = STATE_GAME_OVER;
			break;
		}
		if (state.player.switchOn(mapData)) {
			state.board.velocity.x = 2.0f;
			mapData[46][1] = 252;
		}
		state.board.update(mode, elapsed, mapData, state.player, state.board);
		state.enemies.erase(remove_if(state.enemies.begin(), state.enemies.end(), shouldDie), state.enemies.end());
		if (state.player.exit) {
			state.enemies.clear();
			state.bullets.clear();
			state.board = Entity();
			flag = true;
			mode = STATE_GAME_OVER;
			break;
		}
		state.bullets.erase(remove_if(state.bullets.begin(), state.bullets.end(), shouldRemove), state.bullets.end());
		for (size_t i = 0; i < state.bullets.size(); i++) {
			if (state.player.collide(state.bullets[i])) {
				if (state.bullets[i].parent->type == "enemy") {
					state.enemies.clear();
					state.bullets.clear();
					state.board = Entity();
					Mix_PlayChannel(-1, deadSound, 0);
					mode = STATE_GAME_OVER;
					break;
				}
			}
			for (size_t j = 0; j < state.enemies.size(); j++) {
				if (state.enemies[j].collide(state.bullets[i])) {
					if (state.bullets[i].parent->type == "player") {
						state.bullets[i].collideSide = true;
						state.enemies[j].alive = false;
					}
				}
			}
			state.bullets[i].update(mode, elapsed, mapData, state.player, state.board);
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			if (state.enemies[i].collide(state.player)) {
				state.enemies.clear();
				state.bullets.clear();
				Mix_PlayChannel(-1, deadSound, 0);
				flag = false;
				state.board = Entity();
				mode = STATE_GAME_OVER;
				break;
			}
			if (state.enemies[i].state == ALERT && state.enemies[i].accumalator >= ENEMY_GAP) {
				state.bullets.push_back(state.enemies[i].shoot(bulletSprite));
				state.enemies[i].accumalator -= ENEMY_GAP;
				Mix_PlayChannel(-1, shootSound, 0);
			}
			state.enemies[i].update(mode, elapsed, mapData, state.player, state.board);
		}
		break;
	case STATE_GAME_OVER:
		break;
	}
}

void render(GameState& state, GameMode& mode, ShaderProgram* program, int textureID, int fontTexture, int playerTexture, const Entity& player, const int* runAnimation, const int* jumpAnimation, const int jumpFrames, const int walkFrames, float& walkElapsed, float& jumpElapsed, float framesPerSecond, int& walkIndex, int& jumpIndex, const FlareMap& map, bool& flag, float& elapsed, int**& mapData) {
	switch (mode) {
	case STATE_MAIN_MENU:
		glClear(GL_COLOR_BUFFER_BIT);
		drawText(program, fontTexture, "Welcome to My World!", 0.3f, 0.0f, -2.8f, 1.0f);
		drawText(program, fontTexture, "PLAY", 0.5f, 0.0f, -0.8f, -0.1f);
		drawText(program, fontTexture, "Press Mouse to Start", 0.25f, 0.0f, -2.3f, -1.0f);
		break;
	case STATE_GUIDE_PAGE:
		glClear(GL_COLOR_BUFFER_BIT);
		drawText(program, fontTexture, "Guides", 0.5f, 0.0f, -1.2f, 1.2f);
		drawText(program, fontTexture, "1. Press left or right to move", 0.2f, 0.0f, -3.0f, 0.5f);
		drawText(program, fontTexture, "2. Press space to jump", 0.2f, 0.0f, -3.0f, 0.0f);
		drawText(program, fontTexture, "3. Press A to shoot", 0.2f, 0.0f, -3.0f, -0.5f);
		drawText(program, fontTexture, "4. Press Q to quit", 0.2f, 0.0f, -3.0f, -1.0f);
		drawText(program, fontTexture, "(Tip: Mind the floor!)", 0.2f, 0.0f, -2.0f, -1.5f);
		break;
	case STATE_LEVEL_ONE:
	case STATE_LEVEL_TWO:
		glClear(GL_COLOR_BUFFER_BIT);
		drawTile(program, textureID, map, state.player, mapData);
		if (state.player.velocity.x != 0.0f) {
			if (state.player.velocity.y <= 0.0f) {
				renderPlayer(program, playerTexture, player, runAnimation, walkFrames, walkElapsed, framesPerSecond, elapsed, walkIndex);
			}
			else {
				renderPlayer(program, playerTexture, player, jumpAnimation, jumpFrames, jumpElapsed, framesPerSecond, elapsed, jumpIndex);
			}
		}
		else {
			if (state.player.velocity.y <= 0.0f) {
				state.player.draw(program, state.player);
			}
			else {
				renderPlayer(program, playerTexture, player, jumpAnimation, jumpFrames, jumpElapsed, framesPerSecond, elapsed, jumpIndex);
			}
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			state.enemies[i].draw(program, state.player);
		}
		for (size_t j = 0; j < state.bullets.size(); j++) {
			state.bullets[j].draw(program, state.player);
		}
		break;
	case STATE_LEVEL_THREE:
		glClear(GL_COLOR_BUFFER_BIT);
		drawTile(program, textureID, map, state.player, mapData);
		if (state.player.velocity.x != 0.0f) {
			if (state.player.velocity.y <= 0.0f) {
				renderPlayer(program, playerTexture, player, runAnimation, walkFrames, walkElapsed, framesPerSecond, elapsed, walkIndex);
			}
			else {
				renderPlayer(program, playerTexture, player, jumpAnimation, jumpFrames, jumpElapsed, framesPerSecond, elapsed, jumpIndex);
			}
		}
		else {
			if (state.player.velocity.y <= 0.0f) {
				state.player.draw(program, state.player);
			}
			else {
				renderPlayer(program, playerTexture, player, jumpAnimation, jumpFrames, jumpElapsed, framesPerSecond, elapsed, jumpIndex);
			}
		}
		for (size_t i = 0; i < state.enemies.size(); i++) {
			state.enemies[i].draw(program, state.player);
		}
		for (size_t j = 0; j < state.bullets.size(); j++) {
			state.bullets[j].draw(program, state.player);
		}
		state.board.draw(program, state.player);
		break;
	case STATE_GAME_OVER:
		glClear(GL_COLOR_BUFFER_BIT);
		if (!flag) {
			drawText(program, fontTexture, "YOU LOSE!", 0.4f, 0.1f, -2.0f, 1.0f);
		}
		else {
			drawText(program, fontTexture, "Congratulations!", 0.4f, 0.0f, -3.0f, 1.0f);
		}
		drawText(program, fontTexture, "Play Again?", 0.4f, 0.0f, -2.0f, -0.5f);
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
	Mix_Music* Background;
	Background = Mix_LoadMUS("Background.mp3");
	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GameState state;
	GameMode mode = STATE_MAIN_MENU;
	GLuint textureID = loadTexture("platformer.png");
	GLuint fontTexture = loadTexture("font1.png");
	GLuint playerTexture = loadTexture("playerSprite.png");
	float enemy_u = (float)((373 % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
	float enemy_v = (float)((373 / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y);
	SheetSprite enemySprite = SheetSprite(textureID, enemy_u, enemy_v, 1.0f / SPRITE_COUNT_X, 1.0f / SPRITE_COUNT_Y, TILE_SIZE);
	float bullet_u = (float)((106 % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
	float bullet_v = (float)((106 / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y);
	SheetSprite bulletSprite = SheetSprite(textureID, bullet_u, bullet_v, 1.0f / SPRITE_COUNT_X, 1.0f / SPRITE_COUNT_Y, TILE_SIZE);
	float board_u = (float)((395 % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X);
	float board_v = (float)((395 / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y);
	SheetSprite boardSprite = SheetSprite(textureID, board_u, board_v, 4.0f / SPRITE_COUNT_X, 1.0f / SPRITE_COUNT_Y, TILE_SIZE);
	float player_u = (float)((7 % 7) / (float)7);
	float player_v = (float)((7 / 7) / (float)3);
	SheetSprite playerSprite = SheetSprite(playerTexture, player_u, player_v, 1.0f / 7.0f, 1.0f / 3.0f, TILE_SIZE);
	state.player = Entity(4 * TILE_SIZE + 0.5F * TILE_SIZE, -46 * TILE_SIZE - 0.5F * TILE_SIZE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, TILE_SIZE * 0.75f, TILE_SIZE, 0.0F, "player", playerSprite);
	int** mapData = new int*[LEVEL_HEIGHT];
	for (size_t i = 0; i < LEVEL_HEIGHT; i++) {
		mapData[i] = new int[LEVEL_WIDTH];
	}
	int levelOne[LEVEL_HEIGHT][LEVEL_WIDTH] = { 
	{333,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,311,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,0,0,0,123,123,123,123,123,123,123,0,0,0,123,123,123,123,123,123,123,0,0,0,0,0,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,71,71,0,0,0,71,71,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,333},
	{333,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,71,71,71,71,71,71,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,131,333},
	{333,0,0,0,0,0,0,0,0,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,0,0,0,0,0,0,123,123,123,123,0,0,0,0,0,0,0,0,123,123,333},
	{333,0,0,0,0,0,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,0,0,0,0,123,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,123,123,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,123,123,123,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,131,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,0,0,0,123,123,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,71,71,71,71,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,71,71,71,71,71,71,71,71,123,123,123,71,71,71,71,71,71,71,123,123,123,123} };
	int levelTwo[LEVEL_HEIGHT][LEVEL_WIDTH] = { 
	{333,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,123,123,0,0,0,123,123,123,0,0,0,123,123,123,0,0,0,123,123,123,0,0,0,123,123,123,0,0,0,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,71,71,71,0,0,0,71,71,71,0,0,0,71,71,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,311,333},
	{333,0,0,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,0,0,0,123,123,123,0,0,0,123,123,123,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,127,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,127,153,153,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,71,71,71,0,0,71,71,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,127,153,153,153,153,128,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,0,0,123,123,123,123,123,123,123,123,123,123,123,123,123,123,0,0,0,333},
	{333,0,0,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,127,153,153,153,153,153,153,128,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,127,153,153,153,153,153,153,153,153,128,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,123,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,0,0,123,123,123,123,123,123,123,123,0,0,123,123,123,123,0,0,123,123,123,123,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,124,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,0,333},
	{333,71,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,0,131,333},
	{333,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,123,123,123,123,0,0,0,0,0,123,123,123,123,0,0,0,0,0,123,123,123,123,0,0,0,0,0,123,123,123,123,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,71,0,0,0,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,123,123,123,123,0,0,0,0,0,0,0,0,0,0,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,71,333} };
	int levelThree[LEVEL_HEIGHT][LEVEL_WIDTH] = {
		{333,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,153,153,153,333},
	{333,0,0,0,153,153,153,153,153,153,153,153,153,153,153,153,153,153,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,153,0,311,333},
	{333,0,0,0,153,0,0,0,0,0,153,153,0,0,0,0,0,153,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,123,123,123,333},
	{333,0,0,0,153,15,0,0,0,0,153,153,0,0,0,0,0,153,0,0,0,0,153,123,123,123,123,123,0,0,0,123,123,123,123,123,0,0,0,123,123,123,123,123,0,0,0,123,123,123,153,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,153,123,123,123,0,0,153,153,0,0,123,123,123,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,123,123,123,153,153,123,123,123,0,0,0,0,0,0,0,123,123,123,123,123,123,123,123,123,123,123,153,123,123,123,123,123,123,123,153,123,123,123,123,123,123,123,123,123,0,0,0,0,123,123,123,123,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,71,71,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,123,0,0,0,71,71,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,153,153,153,153,153,0,0,0,0,123,0,0,123,123,123,123,0,0,0,0,0,123,123,123,123,123,123,123,0,0,0,0,0,123,123,123,123,123,123,0,0,0,0,0,123,123,123,123,123,123,0,0,0,0,0,0,0,123,123,123,123,123,333},
	{333,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,123,123,123,123,123,123,123,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,131,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,123,123,123,123,123,333},
	{333,123,123,123,123,123,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,153,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,153,153,0,0,0,0,285,0,0,0,0,0,153,0,0,0,71,0,0,0,71,0,0,153,153,0,0,71,0,0,0,71,0,0,153,153,0,0,71,0,71,0,71,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,123,123,123,123,123,0,0,153,153,0,0,123,123,123,123,123,0,0,0,153,0,0,0,123,123,123,123,123,0,0,153,153,0,0,123,123,123,123,123,0,0,153,153,0,0,123,123,123,123,123,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{333,251,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,333},
	{123,123,123,123,123,123,71,71,71,71,71,123,123,123,123,123,123,71,71,71,71,71,123,123,123,123,123,123,123,71,71,71,71,71,123,123,123,123,123,123,71,71,71,71,71,123,123,123,123,123,123,71,71,71,71,71,123,123,123,123,123,123,123,123} };
	FlareMap map;
	map.Load("levelOne.txt");
	for (size_t i = 0; i < map.entities.size(); i++) {
		placeEntity(state, map.entities[i].type, map.entities[i].x, map.entities[i].y, enemySprite);
	}
	loadLevel(mapData, levelOne);
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
	Mix_PlayMusic(Background, -1);
	while (!done) {
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		render(state, mode, &program, textureID, fontTexture, playerTexture, state.player, runAnimation, jumpAnimation, jumpFrames, walkFrames, walkElapsed, jumpElapsed, framesPerSecond, walkIndex, jumpIndex, map, flag, elapsed, mapData);
		processEvents(state, mode, map, event, done, jumpSound, playerSprite, enemySprite, bulletSprite, levelOne, mapData);
		Update(state, mode, flag, map, elapsed, shootSound, deadSound, playerSprite, enemySprite, bulletSprite, boardSprite, levelTwo, levelThree, mapData);
		SDL_GL_SwapWindow(displayWindow);
	}
	Mix_FreeChunk(jumpSound);
	Mix_FreeChunk(shootSound);
	Mix_FreeChunk(deadSound);
	Mix_FreeMusic(Background);
	SDL_Quit();
	return 0;
}


