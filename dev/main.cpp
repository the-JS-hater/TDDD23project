#include "raylib.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

enum Tile {
    EMPTY,
    TILE,
};

using GameMap = std::vector<std::vector<Tile>>;

int const TILE_SIZE = 64;
GameMap testMap = {
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, TILE,  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, TILE,  TILE,  EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {TILE, TILE, TILE,  TILE,  TILE,  TILE,  TILE,  TILE,  TILE,  TILE,  TILE,  TILE,  TILE,  EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
  {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
};

enum PlayerState {
  GROUNDED = 1 << 0,
  JUMPING  = 1 << 1,
  SLIDING  = 1 << 2,
  DASHING  = 1 << 3,
  DUCKING  = 1 << 4,
};

bool hasFlag(uint32_t flags, PlayerState s) { return (flags & s) != 0; }
void setFlag(uint32_t &flags, PlayerState s) { flags |= s; }
void clearFlag(uint32_t &flags, PlayerState s) { flags &= ~s; }

struct Player {
  float x, y, w, h;          
  float original_h;    
  float dx, dy;        
  float max_vel;

  float accel, drag, gravity, jump_force;
  float dash_timer, slide_timer;

  float dash_speed;
  float dash_duration;
  float slide_duration;
  float duck_scale;

  int facing;
  uint32_t status_flags;
};

void renderLevel(GameMap const& map)
{
  for (unsigned y = 0; y < map.size(); ++y)
  {
    for (unsigned x = 0; x < map[y].size(); ++x)
    {
      if (map[y][x] == TILE) DrawRectangle(
        x * TILE_SIZE,
        y * TILE_SIZE,
        TILE_SIZE,
        TILE_SIZE,
        RED
      );
    }
  }
}

void renderPlayer(Player const& player)
{
  DrawRectangle(
    (int)player.x,
    (int)player.y,
    (int)player.w,
    (int)player.h,
    GREEN
  );
}

void handlePlayerInput(Player& player, float dt)
{
	//TODO: 
	// - controller support
	// - weapon pickups
	// - weapons usage
	
  bool left = IsKeyDown(KEY_A);
  bool right = IsKeyDown(KEY_D);

  if (left) 
	{
    player.dx -= player.accel * dt;
    player.facing = -1;
  }
  if (right) 
	{
    player.dx += player.accel * dt;
    player.facing = 1;
  }
  if (!left && !right) 
	{
    if (std::fabs(player.dx) < 0.05f) player.dx = 0.0f;
    else player.dx *= (1.0f - player.drag * dt);
  }
  player.dx = std::max(-player.max_vel, std::min(player.dx, player.max_vel));

  if (IsKeyPressed(KEY_W) && hasFlag(player.status_flags, GROUNDED)) 
	{
    player.dy = -player.jump_force;
    clearFlag(player.status_flags, GROUNDED);
    setFlag(player.status_flags, JUMPING);
  }
  if (IsKeyPressed(KEY_LEFT_SHIFT) && !hasFlag(player.status_flags, DASHING)) 
	{
    setFlag(player.status_flags, DASHING);
    player.dash_timer = player.dash_duration;
  		
		int dir = player.facing; 
		if (left) dir = -1;
		if (right) dir = 1;

    player.dx = dir * player.dash_speed;
  }
  if (hasFlag(player.status_flags, DASHING)) 
	{
    player.dash_timer -= dt;
    if (player.dash_timer <= 0.0f) 
		{
      clearFlag(player.status_flags, DASHING);
      
			// reduce speed after dash to avoid instant stop
      if (std::fabs(player.dx) > player.max_vel) 
			{
        player.dx = (player.dx > 0 ? player.max_vel : -player.max_vel);
      }
    }
  }
	if (IsKeyDown(KEY_S)) 
	{
	  if (hasFlag(player.status_flags, GROUNDED)) 
		{
	    if (IsKeyPressed(KEY_S) && 
	      !hasFlag(player.status_flags, SLIDING) &&
	      std::fabs(player.dx) > 50.0f //sliding threshold
			) 
	    {
	      setFlag(player.status_flags, SLIDING);
	      player.slide_timer = player.slide_duration;
	      if (!hasFlag(player.status_flags, DUCKING)) 
				{
	        setFlag(player.status_flags, DUCKING);
	        float new_h = player.original_h * player.duck_scale;
	        player.y += (player.h - new_h);
	        player.h = new_h;
	      }
	      player.dx = (player.facing >= 0 ? player.max_vel : -player.max_vel) * 1.2f;
	    }
	    else if (!hasFlag(player.status_flags, SLIDING)) 
			{
	      if (!hasFlag(player.status_flags, DUCKING)) 
				{
	        setFlag(player.status_flags, DUCKING);
	        float new_h = player.original_h * player.duck_scale;
	        player.y += (player.h - new_h);
	        player.h = new_h;
	      }
	    }
	  }
	}
	else 
	{
    if (hasFlag(player.status_flags, DUCKING)) 
		{
      clearFlag(player.status_flags, DUCKING);
      player.y -= (player.original_h - player.h); 
      player.h = player.original_h;
    }
	}
	
	// TODO: probably move this out once i have a proper update player func
  if (!hasFlag(player.status_flags, GROUNDED)) {
      player.dy += player.gravity * dt;
  }

  if (std::fabs(player.dx) < 0.001f) player.dx = 0.0f;
  if (std::fabs(player.dy) < 0.001f) player.dy = 0.0f;
}

bool checkMapCollision(GameMap const& map, Player const& player) {
  Rectangle rect = {
    player.x,
    player.y,
    player.w,
    player.h
  };

  int rows = (int)map.size();
  int cols = map.empty() ? 0 : (int)map[0].size();

  int left   = std::max(0, (int)std::floor(rect.x / TILE_SIZE));
  int right  = std::min(cols - 1, (int)std::floor((rect.x + rect.width) / TILE_SIZE));
  int top    = std::max(0, (int)std::floor(rect.y / TILE_SIZE));
  int bottom = std::min(rows - 1, (int)std::floor((rect.y + rect.height) / TILE_SIZE));

  for (int y = top; y <= bottom; ++y) 
	{
    for (int x = left; x <= right; ++x) 
		{
      if (map[y][x] == TILE) {
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
  InitWindow(720, 480, "Game (fixed)");
  SetTargetFPS(60);
  HideCursor();

  int const res_w = 1920;
  int const res_h = 1080;
  RenderTexture2D renderTarget = LoadRenderTexture(res_w, res_h);
	
	//TODO: player constructor
  Player player1 = {};
  player1.x = 10.0f;
  player1.y = 10.0f;
  player1.w = 20.0f;
  player1.h = 40.0f;
  player1.original_h = player1.h;
  player1.dx = 0.0f;
  player1.dy = 0.0f;
  player1.max_vel = 300.0f; 

  player1.accel = 1200.0f; 
  player1.drag = 6.0f;     
  player1.gravity = 2000.0f; 
  player1.jump_force = 700.0f;

  player1.dash_speed = 700.0f;
  player1.dash_duration = 0.18f; 
  player1.slide_duration = 0.6f;
  player1.duck_scale = 0.5f;

  player1.dash_timer = 0.0f;
  player1.slide_timer = 0.0f;

  player1.facing = 1;

  player1.status_flags = 0;
  setFlag(player1.status_flags, GROUNDED); 

  while (!WindowShouldClose())
  {
    float dt = GetFrameTime();

    handlePlayerInput(player1, dt);

    float move_x = player1.dx * dt;
    player1.x += move_x;
    if (checkMapCollision(testMap, player1)) 
		{
      player1.x -= move_x;
      player1.dx = 0.0f;
    }
    float move_y = player1.dy * dt;
    player1.y += move_y;
    if (checkMapCollision(testMap, player1)) 
		{
      player1.y -= move_y;
      if (move_y > 0.0f) 
			{
        player1.dy = 0.0f;
        setFlag(player1.status_flags, GROUNDED);
        clearFlag(player1.status_flags, JUMPING);
      } 
			else if (move_y < 0.0f) 
			{
        player1.dy = 0.0f;
        clearFlag(player1.status_flags, JUMPING);
      }
    } 
		else 
		{
      clearFlag(player1.status_flags, GROUNDED);
    }

    BeginTextureMode(renderTarget);
    ClearBackground(BLACK);
    BeginDrawing();
    renderLevel(testMap);
    renderPlayer(player1);
    EndDrawing();
    EndTextureMode();

    renderToScreen(renderTarget);
  }
  UnloadRenderTexture(renderTarget);
  CloseWindow();
}
