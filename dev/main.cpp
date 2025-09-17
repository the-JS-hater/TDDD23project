#include "raylib.h"
#include "stdio.h"
#include <vector>
#include <algorithm>
#include <cmath>


enum Tile {
	EMPTY,
	TILE,
};

using GameMap = std::vector<std::vector<Tile>>;

int const TILE_SIZE = 32; 
GameMap testMap = {
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, TILE, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, TILE, TILE, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
	{EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
};

struct Player {
	float x, y, w, h, dx, dy, max_vel;
};


void renderLevel(GameMap const& map)
{
	for (unsigned y = 0; y < map.size(); ++y) 
	{
		for (unsigned x = 0; x < map[y].size(); ++x)
		{
			if (map[y][x]) DrawRectangle(
				x * TILE_SIZE,
				y * TILE_SIZE,
				TILE_SIZE,
				TILE_SIZE,
				RED
			);
		}
	}
}

void renderPlayer(Player player)
{
	DrawRectangle(
		player.x,
		player.y,
		player.w,
		player.h,
		GREEN
	);
}

void handlePlayerInput(Player& player)
{
	//TODO: 
	// -jump 
	// -slide 
	// -dash 
	// -duck
		
	//TODO: controller support
	
	//TODO: pickup objects

	float const accel = 1.0f;
	float const drag = 0.2f;
	
	if (IsKeyDown(KEY_W)) player.dy -= accel;
	else if (IsKeyDown(KEY_S)) player.dy += accel;
	else {
	  if (player.dy > 0.0f) player.dy = std::max(0.0f, player.dy - drag);
	  else if (player.dy < 0.0f) player.dy = std::min(0.0f, player.dy + drag);
	}
	if (IsKeyDown(KEY_A)) player.dx -= accel;
	else if (IsKeyDown(KEY_D)) player.dx += accel;
	else {
	  if (player.dx > 0.0f) player.dx = std::max(0.0f, player.dx - drag);
	  else if (player.dx < 0.0f) player.dx = std::min(0.0f, player.dx + drag);
	}
	player.dx = std::max(-player.max_vel, std::min(player.dx, player.max_vel));
	player.dy = std::max(-player.max_vel, std::min(player.dy, player.max_vel));
	
	if (std::fabs(player.dx) <= 0.2f) player.dx = 0.0f;
	if (std::fabs(player.dy) <= 0.2f) player.dy = 0.0f;
}

bool checkMapCollision(GameMap const& map, Player player) {
	Rectangle rect = {
		player.x,
		player.y,
		player.w,
		player.h
	};

  int rows = map.size();
  int cols = map.empty() ? 0 : map[0].size();

  int left   = std::max(0, (int)(rect.x / TILE_SIZE));
  int right  = std::min(cols - 1, (int)((rect.x + rect.width) / TILE_SIZE));
  int top    = std::max(0, (int)(rect.y / TILE_SIZE));
  int bottom = std::min(rows - 1, (int)((rect.y + rect.height) / TILE_SIZE));

  for (int y = top; y <= bottom; y++) 
	{
  	for (int x = left; x <= right; x++) {
      if (map[y][x]) 
			{ 
        Rectangle tile = {
					(float)x * TILE_SIZE, 
					(float)y * TILE_SIZE,
          (float)TILE_SIZE, 
					(float)TILE_SIZE 
				};
            
				if (CheckCollisionRecs(rect, tile)) return true;
      }
    }
  }
  return false;
}

void renderToScreen(RenderTexture2D renderTarget) {
	ClearBackground(RAYWHITE);

	DrawTexturePro(
		renderTarget.texture,
		Rectangle{ 
			0, 0, 
			(float)renderTarget.texture.width, 
			-(float)renderTarget.texture.height
		}, 
		Rectangle{ 
			0, 0, 
			(float)GetScreenWidth(), 
			(float)GetScreenHeight()
		}, 
		Vector2{ 0, 0 },
		0.0f,
		WHITE
	);
}

int main()
{
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(720, 480, "Game");
	SetTargetFPS(60);
	HideCursor();
	
	int const res_w = 1920;
	int const res_h = 1080;
	RenderTexture2D renderTarget = LoadRenderTexture(res_w, res_h);
	
	Player player1 = {
		10.0f, 10.0f, 20.0f, 40.0f, 0.0f, 0.0f, 5.0f
	};


	while(!WindowShouldClose())
	{
		// INPUT 
		handlePlayerInput(player1);
		
		// UPDATE
		
		//TODO: axis independent collision check
		player1.x += player1.dx;
		player1.y += player1.dy;
		if (checkMapCollision(testMap, player1)) {
			player1.x -= player1.dx;
			player1.y -= player1.dy;
		}

		// RENDER
		//TODO: dynamic camera
		
		// rendering to texture
		{
			BeginTextureMode(renderTarget);
			
			ClearBackground(BLACK);
			BeginDrawing();
			renderLevel(testMap);
			renderPlayer(player1);
			EndDrawing();
			
			EndTextureMode();
		}
		renderToScreen(renderTarget);
	}
	CloseWindow();
}

