#include "raylib.h"
#include "stdio.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

Texture testDudeTex;
Texture testBoxBunny; 

int const RES_W = 1920;
int const RES_H = 1080;

enum Tile {
  VOID,
  TILE,
};

using GameMap = std::vector<std::vector<Tile>>;

int const TILE_SIZE = 64;
GameMap testMap = {
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, TILE, TILE, TILE, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, TILE, TILE, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, TILE, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, TILE, TILE, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE, TILE},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
    {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
};

enum PlayerState {
  GROUNDED = 1 << 0,
  JUMPING = 1 << 1,
  SLIDING = 1 << 2,
  DASHING = 1 << 3,
  DUCKING = 1 << 4,
};

bool hasFlag(uint32_t flags, PlayerState s) { return (flags & s) != 0; }
void setFlag(uint32_t &flags, PlayerState s) { flags |= s; }
void clearFlag(uint32_t &flags, PlayerState s) { flags &= ~s; }

struct Gun {
  float x, y, w, h;
  int ammo;
  float fire_rate;
  float projectile_speed;
  float spread;
  float range;
  bool picked_up = false;
  float cooldown = 0.0f;
};

struct Projectile {
  float x, y;
  float dx, dy;
  float traveled = 0.0f;
  float max_distance;
};

struct Controls {
    int deviceId; 
    int left, right, up, down, jump, dash, fire;
};

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

  Gun *gun = nullptr;

  int facing;
  uint32_t status_flags;

	Controls controls;
};

bool isActionDown(Controls const &c, int action) {
    if (c.deviceId == -1) {
        return IsKeyDown(action);
    } else {
        return IsGamepadButtonDown(c.deviceId, action);
    }
}

bool isActionPressed(Controls const &c, int action) {
    if (c.deviceId == -1) {
        return IsKeyPressed(action);
    } else {
        return IsGamepadButtonPressed(c.deviceId, action);
    }
}

bool isActionReleased(Controls const &c, int action) {
    if (c.deviceId == -1) {
        return IsKeyReleased(action);
    } else {
        return IsGamepadButtonReleased(c.deviceId, action);
    }
}

void renderLevel(GameMap const &map) {
  for (unsigned y = 0; y < map.size(); ++y) {
    for (unsigned x = 0; x < map[y].size(); ++x) {
      if (map[y][x] == TILE)
        DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);
    }
  }
}

void renderPlayer(Player const &player) {
    Rectangle src = {
        0.0f, 
        0.0f, 
        (float)testBoxBunny.width, 
        (float)testBoxBunny.height
    };

    Rectangle dst = {
        player.x, 
        player.y, 
        player.w, 
        player.h
    };
    
		if (player.facing < 0) {
        src.width = -src.width;
    }

    Vector2 origin = {0.0f, 0.0f};

    DrawTexturePro(testBoxBunny, src, dst, origin, 0.0f, WHITE);
		
		float shoulderY = player.y + player.h * 0.30;
    float shoulderX = (player.facing < 0) ? player.x + player.w : player.x;

    float armThickness = player.w * 0.50;       
    float armLength    = player.h * 0.30;       

    if (!player.gun) {
        Rectangle arm = {
            shoulderX - armThickness * 0.5f,
            shoulderY,
            armThickness,
            armLength
        };
        DrawRectangleRec(arm, WHITE);
    } else {
        float armX = (player.facing == 1) ? shoulderX : shoulderX - armLength;
        Rectangle arm = {
            armX,
            shoulderY - armThickness * 0.5f,
            armLength,
            armThickness
        };
        DrawRectangleRec(arm, WHITE);

        Gun const *gun = player.gun;
        float gunScale = player.h / 100.0f; 
        float gunW = gun->w * gunScale;
        float gunH = gun->h * gunScale;

        float gunX = (player.facing == 1) ? arm.x + arm.width : arm.x - gunW;
        float gunY = arm.y + arm.height/2.0f - gunH/2.0f;

        Rectangle gunRect = {gunX, gunY, gunW, gunH};
        DrawRectangleRec(gunRect, BLACK);
    }
}


bool hasMapCollision(GameMap const &map, Player const &player) {
  Rectangle rect = {player.x, player.y, player.w, player.h};

  int rows = (int)map.size();
  int cols = map.empty() ? 0 : (int)map[0].size();

  int left = std::max(0, (int)std::floor(rect.x / TILE_SIZE));
  int right =
      std::min(cols - 1, (int)std::floor((rect.x + rect.width) / TILE_SIZE));
  int top = std::max(0, (int)std::floor(rect.y / TILE_SIZE));
  int bottom =
      std::min(rows - 1, (int)std::floor((rect.y + rect.height) / TILE_SIZE));

  for (int y = top; y <= bottom; ++y) {
    for (int x = left; x <= right; ++x) {
      if (map[y][x] == TILE) {
        Rectangle tile = {(float)x * TILE_SIZE, (float)y * TILE_SIZE,
                          (float)TILE_SIZE, (float)TILE_SIZE};
        if (CheckCollisionRecs(rect, tile))
          return true;
      }
    }
  }
  return false;
}

void handlePlayerCollision(Player &player, GameMap const &map, float const dt) {
  float move_x = player.dx * dt;
  player.x += move_x;
  if (hasMapCollision(testMap, player)) {
    player.x -= move_x;
    player.dx = 0.0f;
  }
  float move_y = player.dy * dt;
  player.y += move_y;
  if (hasMapCollision(testMap, player)) {
    player.y -= move_y;
    if (move_y > 0.0f) {
      player.dy = 0.0f;
      setFlag(player.status_flags, GROUNDED);
      clearFlag(player.status_flags, JUMPING);
    } else if (move_y < 0.0f) {
      player.dy = 0.0f;
    }
  } else {
    clearFlag(player.status_flags, GROUNDED);
  }
}


void handlePlayerInput(Player &player, float dt) {
	bool left  = isActionDown(player.controls, player.controls.left);
	bool right = isActionDown(player.controls, player.controls.right);
  
	float accel_mod = (hasFlag(player.status_flags, DUCKING) &&
                     hasFlag(player.status_flags, GROUNDED))
                        ? 0.2f
                        : 1.0f;
  if (!hasFlag(player.status_flags, SLIDING)) {
    if (left) {
      player.dx -= player.accel * dt;
      player.facing = -1;
    }
    if (right) {
      player.dx += player.accel * dt;
      player.facing = 1;
    }
    if (!left && !right) {
      if (std::fabs(player.dx) < 0.05f)
        player.dx = 0.0f;
      else
        player.dx *= (1.0f - player.drag * dt);
    }
  } else {
    float const sliding_drag_reduction = 0.2f;
    player.dx *= (1.0f - player.drag * sliding_drag_reduction * dt);
  }

  player.dx =
      std::clamp(player.dx, -player.max_vel, player.max_vel * accel_mod);
	
	if (isActionDown(player.controls, player.controls.jump) && hasFlag(player.status_flags, GROUNDED)) {
	    player.dy = -player.jump_force;
	    clearFlag(player.status_flags, GROUNDED);
	    setFlag(player.status_flags, JUMPING);
	}

  if (isActionReleased(player.controls, player.controls.jump) && player.dy < -player.jump_force * 0.5f) {
    player.dy *= 0.5f; 
  }

  if (isActionPressed(player.controls, player.controls.dash) && !hasFlag(player.status_flags, DASHING)) {
    setFlag(player.status_flags, DASHING);
    player.dash_timer = player.dash_duration;

    int dir = player.facing;
    if (left)
      dir = -1;
    if (right)
      dir = 1;

    player.dx = dir * player.dash_speed;
  }
  if (hasFlag(player.status_flags, DASHING)) {
    player.dash_timer -= dt;
    if (player.dash_timer <= 0.0f) {
      clearFlag(player.status_flags, DASHING);
      if (std::fabs(player.dx) > player.max_vel) {
        player.dx = (player.dx > 0 ? player.max_vel : -player.max_vel);
      }
    }
  }

  float const sliding_threshold = 20.0f;
  if (isActionDown(player.controls, player.controls.down)) {
    if (hasFlag(player.status_flags, GROUNDED)) {
      if (!hasFlag(player.status_flags, SLIDING) &&
          std::fabs(player.dx) > sliding_threshold) {
        setFlag(player.status_flags, SLIDING);
        float new_h = player.original_h * player.duck_scale;
        player.y += (player.h - new_h);
        player.h = new_h;
        player.slide_timer = player.slide_duration;
      }
    }
    if (!hasFlag(player.status_flags, SLIDING) &&
        !hasFlag(player.status_flags, DUCKING)) {
      setFlag(player.status_flags, DUCKING);
      float new_h = player.original_h * player.duck_scale;
      player.y += (player.h - new_h);
      player.h = new_h;
    }
  } else {
    float old_h = player.h;
    float new_h = player.original_h;
    float diff = new_h - old_h;

    Player test = player;
    test.y -= diff;
    test.h = new_h;

    if (!hasMapCollision(testMap, test)) {
      clearFlag(player.status_flags, DUCKING);
      clearFlag(player.status_flags, SLIDING);
      player.y -= diff;
      player.h = new_h;
    } else {
      setFlag(player.status_flags, DUCKING);
    }
  }

  if (hasFlag(player.status_flags, SLIDING)) {
    player.slide_timer -= dt;

    if (player.slide_timer <= 0.0f || std::fabs(player.dx) < 30.0f) {
      clearFlag(player.status_flags, SLIDING);
    }
  }

  if (!hasFlag(player.status_flags, GROUNDED)) {
    player.dy += player.gravity * dt;
  }

  if (std::fabs(player.dx) < 0.001f)
    player.dx = 0.0f;
  if (std::fabs(player.dy) < 0.001f)
    player.dy = 0.0f;
}

void handleGunPickups(Player &player, std::vector<Gun> &guns) {
  if (player.gun) {
		return;
	}
	Rectangle playerRect = {player.x, player.y, player.w, player.h};
  for (Gun &gun : guns) {
    if (!gun.picked_up) {
      Rectangle gunRect = {gun.x, gun.y, gun.w, gun.h};
      if (CheckCollisionRecs(playerRect, gunRect)) {
        gun.picked_up = true;
        player.gun = &gun;
        break;
      }
    }
  }
}

void handleShooting(Player &player, std::vector<Projectile> &projectiles,
                    float dt) {
  if (!player.gun) {
    return;
  }

  Gun *gun = player.gun;
	if (gun->ammo <= 0) {
		player.gun = nullptr;
		return;
	}

  if (gun->cooldown > 0.0f) {
    gun->cooldown -= dt;
  }

  if (isActionDown(player.controls, player.controls.fire) && gun->ammo > 0 && gun->cooldown <= 0.0f) {
    gun->cooldown = 1.0f / gun->fire_rate;
    gun->ammo--;

    float baseAngle = (player.facing == -1) ? M_PI : 0.0f;
    float speed_factor = std::min(1.0f, std::fabs(player.dx) / player.max_vel);
    float spread_angle = gun->spread * (1.0f + speed_factor * 2.0f);

    float angle =
        baseAngle + ((rand() / (float)RAND_MAX) - 0.5f) * spread_angle;

    float vx = cosf(angle) * gun->projectile_speed;
    float vy = sinf(angle) * gun->projectile_speed;

    projectiles.push_back({player.x + player.w, player.y + (int)(player.h * 0.30), vx,
                           vy, 0.0f, gun->range});
  }
}

Gun spawnRandomGun(GameMap const &map, int screenWidth, int screenHeight) {
    Gun gun = {};
    gun.w = 60;
    gun.h = 30;
    gun.ammo = 30;
    gun.fire_rate = 5.0f;
    gun.projectile_speed = 800.0f;
    gun.spread = 0.15f;
    gun.range = 600.0f;
    gun.picked_up = false;
    gun.cooldown = 0.0f;

    while (true) {
        int x = GetRandomValue(0, screenWidth - gun.w);
        int y = GetRandomValue(0, screenHeight - gun.h);

        Rectangle rect = {(float)x, (float)y, gun.w, gun.h};

        bool collision = false;
        int rows = (int)map.size();
        int cols = map.empty() ? 0 : (int)map[0].size();

        int left = std::max(0, (int)std::floor(rect.x / TILE_SIZE));
        int right =
            std::min(cols - 1, (int)std::floor((rect.x + rect.width) / TILE_SIZE));
        int top = std::max(0, (int)std::floor(rect.y / TILE_SIZE));
        int bottom =
            std::min(rows - 1, (int)std::floor((rect.y + rect.height) / TILE_SIZE));

        for (int yy = top; yy <= bottom && !collision; ++yy) {
            for (int xx = left; xx <= right && !collision; ++xx) {
                if (map[yy][xx] == TILE) {
                    Rectangle tile = {(float)xx * TILE_SIZE, (float)yy * TILE_SIZE,
                                      (float)TILE_SIZE, (float)TILE_SIZE};
                    if (CheckCollisionRecs(rect, tile)) {
                        collision = true;
                    }
                }
            }
        }
        if (!collision) {
            gun.x = (float)x;
            gun.y = (float)y;
            break;
        }
    }
    return gun;
}


void updateProjectiles(std::vector<Projectile> &projectiles, float dt,
                       GameMap const &map) {
  for (size_t i = 0; i < projectiles.size();) {
    Projectile &p = projectiles[i];
    float move_x = p.dx * dt;
    float move_y = p.dy * dt;
    p.x += move_x;
    p.y += move_y;
    p.traveled += sqrtf(move_x * move_x + move_y * move_y);

    Rectangle rect = {p.x, p.y, 8, 8};
    if (p.traveled >= p.max_distance ||
        hasMapCollision(map, *(Player *)&rect)) {
      projectiles[i] = projectiles.back();
      projectiles.pop_back();
      continue;
    }
    i++;
  }
}

void renderGuns(std::vector<Gun> const &guns) {
  for (auto const &gun : guns) {
    if (!gun.picked_up) {
      DrawRectangle(gun.x, gun.y, gun.w, gun.h, BLUE);
    }
  }
}

void renderProjectiles(std::vector<Projectile> const &projectiles) {
  for (auto const &p : projectiles) {
    DrawRectangle((int)p.x, (int)p.y, 8, 8, YELLOW);
  }
}

void renderToScreen(RenderTexture2D renderTarget) {
  ClearBackground(BLACK);

  float scale = std::min(
      (float)GetScreenWidth() / RES_W,
      (float)GetScreenHeight() / RES_H
  );

  float scaledWidth  = RES_W * scale;
  float scaledHeight = RES_H * scale;
  float offsetX = (GetScreenWidth() - scaledWidth) / 2;
  float offsetY = (GetScreenHeight() - scaledHeight) / 2;

  Rectangle src = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
  Rectangle dst = {offsetX, offsetY, scaledWidth, scaledHeight};

  DrawTexturePro(renderTarget.texture, src, dst, {0, 0}, 0.0f, WHITE);
}

Player initPlayer() {
  Player player = {};
  player.x = 10.0f;
  player.y = 10.0f;
  player.w = 75.0f;
  player.h = 100.0f;
  player.original_h = player.h;
  player.dx = 0.0f;
  player.dy = 0.0f;
  player.max_vel = 300.0f;

  player.accel = 1200.0f;
  player.drag = 6.0f;
  player.gravity = 2000.0f;
  player.jump_force = 1000.0f;

  player.dash_speed = 3000.0f;
  player.dash_duration = 1.00;
  player.slide_duration = 0.5f;
  player.duck_scale = 0.3f;

  player.dash_timer = 0.0f;
  player.slide_timer = 0.0f;

  player.facing = 1;

  player.status_flags = 0;
  setFlag(player.status_flags, GROUNDED);

  return player;
}


void init_resources(){
	testDudeTex = LoadTexture("resources/dude75x100.png");
	testBoxBunny = LoadTexture("resources/boxRabbit40x100.png");
}


int main() {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(1080, 720, "Game");
  SetTargetFPS(60);
  HideCursor();
	
	init_resources();

  RenderTexture2D renderTarget = LoadRenderTexture(RES_W, RES_H);
	
  std::vector<Gun> guns;
  std::vector<Projectile> projectiles;
  
	Player player0 = initPlayer();
	Controls keyboard0 = {
    -1,             
    KEY_A, KEY_D,   
    KEY_W, KEY_S,   
    KEY_SPACE,      
    KEY_LEFT_SHIFT, 
    KEY_J           
	};
	Controls gamepad0 = {
	    0,  
	    GAMEPAD_BUTTON_LEFT_FACE_LEFT,
	    GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
	    GAMEPAD_BUTTON_LEFT_FACE_UP,
	    GAMEPAD_BUTTON_LEFT_FACE_DOWN,
	    GAMEPAD_BUTTON_RIGHT_FACE_DOWN, 
	    GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
	    GAMEPAD_BUTTON_RIGHT_TRIGGER_1  
	};
	player0.controls = keyboard0;

	for (int i = 0; i < 3; i++) {
	    guns.push_back(spawnRandomGun(testMap, RES_W, RES_H));
	}


  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    handlePlayerInput(player0, dt);
    handlePlayerCollision(player0, testMap, dt);
    handleGunPickups(player0, guns);
    handleShooting(player0, projectiles, dt);
    updateProjectiles(projectiles, dt, testMap);

    BeginTextureMode(renderTarget);
    ClearBackground(BLACK);
    BeginDrawing();
    renderLevel(testMap);
    renderPlayer(player0);
    renderGuns(guns);
    renderProjectiles(projectiles);
    EndDrawing();
    EndTextureMode();

    renderToScreen(renderTarget);
  }
  UnloadRenderTexture(renderTarget);
  CloseWindow();
}
