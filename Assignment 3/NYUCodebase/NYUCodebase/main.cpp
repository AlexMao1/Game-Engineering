#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include "Matrix.h"
#include "ShaderProgram.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define ENEMY_GAP 2.0f
#define PLAYER_GAP 0.3f

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

void DrawText(ShaderProgram* program, int fontTexture, const string& text, float size, float spacing, float start_x, float start_y) {
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
	Entity(float x, float y, float z, float displ_x, float displ_y, float displ_z, float velocity_x, float velocity_y, float velocity_z, float size_x, float size_y, float size_z, const string& Type, SheetSprite& mySprite) {
		position = Vector3(x, y, z);
		displacement = Vector3(displ_x, displ_y, displ_z);
		velocity = Vector3(velocity_x, velocity_y, velocity_z);
		size = Vector3(size_x, size_y, size_z);
		type = Type;
		sprite = mySprite;
		timeAlive = 0.0f;
	}
	void draw(ShaderProgram* program) {
		Matrix modelMatrix;
		Matrix projectionMatrix;
		Matrix viewMatrix;
		projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
		glUseProgram(program->programID);
		glBindTexture(GL_TEXTURE_2D, sprite.textureID);
		modelMatrix.Identity();
		modelMatrix.Translate(position.x + displacement.x, position.y + displacement.y, position.z + displacement.z);
		program->SetModelMatrix(modelMatrix);
		program->SetProjectionMatrix(projectionMatrix);
		program->SetViewMatrix(viewMatrix);
		float aspect = sprite.width / sprite.height;
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
	void update(float elapsed) {
		if (type == "bullet") {
			timeAlive += elapsed;
			displacement.y += elapsed * velocity.y;
		}
		else if (type == "enemy"){
			if (displacement.x >= 1.5f) {
				velocity.x = -2.0f;
				displacement.x += elapsed * velocity.x;
			}
			else if (displacement.x <= -1.5f) {
				velocity.x = 2.0f;
				displacement.x += elapsed * velocity.x;
			}
			else {
				displacement.x += elapsed * velocity.x;
			}
		}
		else {
				displacement.x += elapsed * velocity.x;
		}
	}
	Entity shoot(SheetSprite& bullet) {
		float aspect = bullet.width / bullet.height;
		if (type == "player") {
			Entity newBullet = Entity(position.x + displacement.x, position.y + 0.5f * size.y + 0.5f * bullet.size + 0.01f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.0f, bullet.size * aspect, bullet.size, 0.0f, "bullet", bullet);
			return newBullet;
		}
		else if (type == "enemy") {
			Entity newBullet = Entity(position.x + displacement.x, position.y - 0.5f * size.y - 0.5f * bullet.size - 0.01f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.5f, 0.0f, bullet.size * aspect, bullet.size, 0.0f, "bullet", bullet);
			return newBullet;
		}
	}
	bool collideWith(const Entity& other) {
		if (position.x + 0.5f * size.x + displacement.x < other.position.x - 0.5f * other.size.x + other.displacement.x||
			position.x - 0.5f * size.x + displacement.x > other.position.x + 0.5f * other.size.x + other.displacement.x||
			position.y + 0.5f * size.y + displacement.y < other.position.y - 0.5f * other.size.y + other.displacement.y||
			position.y - 0.5f * size.y + displacement.y > other.position.y + 0.5f * other.size.y + other.displacement.y) {
			return false;
		}
		else {
			return true;
		}
	}
	Vector3 position;
	Vector3 displacement;
	Vector3 velocity;
	Vector3 size;
	float timeAlive;
	SheetSprite sprite;
	string type;
};

class GameState {
public:
	GameState() {}
	Entity player;
	vector<Entity> enemies;
	vector<Entity> bullets;
};

bool shouldRemove(Entity& bullet) {
	if (bullet.timeAlive > 2.0f) {
		return true;
	}
	else {
		return false;
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
	glClearColor(0.4f, 0.2f, 0.4f, 1.0f);
}

enum GameMode {STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER};

void processEvents(GameMode& mode, GameState& state, SDL_Event& event, bool& done, float& elapsed, const Uint8* keys) {
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
						mode = STATE_GAME_LEVEL;
					}
				}
			}
			break;
		case STATE_GAME_LEVEL:
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
					done = true;
				}
			}
			if (keys[SDL_SCANCODE_LEFT]) {
				state.player.velocity.x = -2.0f;
			}
			else if (keys[SDL_SCANCODE_RIGHT]) {
				state.player.velocity.x = 2.0f;
			}
			break;
		case STATE_GAME_OVER:
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
					done = true;
				}
			}
			break;
	}
}

void Update(GameMode& mode, GameState& state, ShaderProgram* program, SheetSprite& bullet, bool& flag, float& elapsed, float& accumalator, float& playerCounter) {
	switch (mode) {
		case STATE_MAIN_MENU:
			break;
		case STATE_GAME_LEVEL:
			accumalator += elapsed;
			playerCounter += elapsed;
			state.bullets.erase(remove_if(state.bullets.begin(), state.bullets.end(), shouldRemove), state.bullets.end());
			for (size_t i = 0; i < state.bullets.size(); i++) {
				if (state.bullets[i].collideWith(state.player)) {
					mode = STATE_GAME_OVER;
				}
				for (size_t j = 0; j < state.enemies.size(); j++) {
					if (state.bullets[i].collideWith(state.enemies[j])) {
						if (state.bullets[i].velocity.y == 1.5f){
							state.enemies.erase(state.enemies.begin() + j);
							state.bullets.erase(state.bullets.begin() + i);
							if (state.enemies.size() == 0) {
								flag = true;
								mode = STATE_GAME_OVER;
							}
						}
					}
				}
				state.bullets[i].update(elapsed);
			}
			for (size_t i = 0; i < state.enemies.size(); i++) {
				state.enemies[i].update(elapsed);
			}
			if (playerCounter > PLAYER_GAP) {
				state.bullets.push_back(state.player.shoot(bullet));
				playerCounter -= PLAYER_GAP;
			}
			state.player.update(elapsed);
			state.player.velocity.x = 0.0f;
			if (accumalator > ENEMY_GAP) {
				for (size_t i = 0; i < state.enemies.size(); i++) {
					state.bullets.push_back(state.enemies[i].shoot(bullet));
				}
				accumalator -= ENEMY_GAP;
			}
			break;
		case STATE_GAME_OVER:
			break;
	}
}

void render(GameMode& mode, GameState& state, ShaderProgram* program, GLuint& fontTexture, bool& flag) {
	switch (mode) {
		case STATE_MAIN_MENU:
			glClear(GL_COLOR_BUFFER_BIT);
			DrawText(program, fontTexture, "Welcome to Space Invadors!", 0.2f, 0.1f, -3.45f, 1.0f);
			DrawText(program, fontTexture, "PLAY", 0.5f, 0.08f, -0.8f, -0.5f);
			break;
		case STATE_GAME_LEVEL:
			glClear(GL_COLOR_BUFFER_BIT);
			state.player.draw(program);
			for (size_t i = 0; i < state.enemies.size(); i++) {
				state.enemies[i].draw(program);
			}
			for (size_t i = 0; i < state.bullets.size(); i++) {
				state.bullets[i].draw(program);
			}
			break;
		case STATE_GAME_OVER:
			glClear(GL_COLOR_BUFFER_BIT);
			if (!flag) {
				DrawText(program, fontTexture, "YOU LOSE!", 0.4f, 0.1f, -2.0f, 0.0f);
			}
			else {
				DrawText(program, fontTexture, "Congratulations!", 0.4f, 0.0f, -3.0f, 0.0f);
			}
			break;
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
	GameMode mode = STATE_MAIN_MENU;
	GLuint fontTexture = LoadTexture(RESOURCE_FOLDER"font1.png");
	GLuint spriteTexture = LoadTexture(RESOURCE_FOLDER"sheet.png");
	SheetSprite playerShip = SheetSprite(spriteTexture, 211.0f / 1024.f, 941.0f / 1024.0f, 99.0f / 1024.0f, 75.0f / 1024.0f, 0.4f);
	SheetSprite enemy = SheetSprite(spriteTexture, 425.0f / 1024.0f, 552.0f / 1024.0f, 93.0f / 1024.0f, 84.0f / 1024.0f, 0.2f);
	SheetSprite bullet = SheetSprite(spriteTexture, 827.0f / 1024.0f, 125.0f / 1024.0f, 16.0f / 1024.0f, 40.0f / 1024.0f, 0.2f);
	float initial_enemy_x = -1.5f;
	float initial_enemy_y = 1.7f;
	float enemyAspect = enemy.width / enemy.height;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			state.enemies.push_back(Entity(initial_enemy_x + j * (enemy.size * enemyAspect + 0.8f), initial_enemy_y - i * (enemy.size + 0.2f), 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, enemy.size * enemyAspect, enemy.size, 0.0f, "enemy", enemy));
		}
	}
	float playerAspect = playerShip.width / playerShip.height;
	state.player = Entity(0.0f, -1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, playerShip.size * playerAspect, playerShip.size, 0.0f, "player", playerShip);
	bool done = false;
	bool flag = false;
	SDL_Event event;
	float lastFrameTicks = 0.0f;
	float accumalator = 0.0f;
	float playerCounter = 0.0f;
	while (!done) {
		const Uint8* keys = SDL_GetKeyboardState(NULL);
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		render(mode, state, &program, fontTexture, flag);
		processEvents(mode, state, event, done, elapsed, keys);
		Update(mode, state, &program, bullet, flag, elapsed, accumalator, playerCounter);
		SDL_GL_SwapWindow(displayWindow);
	}
	SDL_Quit();
	return 0;
}
