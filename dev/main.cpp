#include "raylib.h"
#include "raymath.h"
#include "stdio.h"
#include "float.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdint>
#include <vector>

Texture testDudeTex;
Texture testBoxBunny; 
Texture woodBoxTex; 

int const RES_W = 1920;
int const RES_H = 1080;

enum Tile {
  VOID,
  TILE,
};

using GameMap = std::vector<std::vector<Tile>>;

int const TILE_SIZE = 64;

enum PlayerState {
  GROUNDED 	= 1 << 0,
  JUMPING 	= 1 << 1,
  SLIDING 	= 1 << 2,
  DASHING 	= 1 << 3,
  DUCKING 	= 1 << 4,
  ALIVE 		= 1 << 5,
};

bool hasFlag(uint32_t flags, PlayerState s) { return (flags & s) != 0; }
void setFlag(uint32_t &flags, PlayerState s) { flags |= s; }
void clearFlag(uint32_t &flags, PlayerState s) { flags &= ~s; }

enum PickupType { GUN, GRENADE };

struct Pickup {
    PickupType type;
    Vector2 position;
    bool active = true;

    int gunId = -1;       
    int grenadeAmount = 1;

		int w = 20;
		int h = 20;
};

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
	int ownerId;
	std::vector<Vector2> trail;
};

struct Controls {
    int deviceId; 
    int left, right, up, down, jump, dash, fire, grenade, interact;
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

	int health;
  int max_health;
	int id;

  Gun *gun = nullptr;
	int kills = 0;
  float hitTimer = 0.0f;
	float respawnTimer = 0.0f;
  int facing;
  uint32_t status_flags;

	int grenadeCount = 2;
	int maxGrenades = 3;
	bool canInteract = false;
	int nearbyPickupIndex = -1;

	Controls controls;
};

struct Grenade {
    float x, y;
    float dx, dy;
    float radius;
    float fuse;              
    float bounce;            
    bool exploded = false;
    std::vector<Vector2> trail;
};

enum GameState {
    ROUND_ACTIVE,
    ROUND_OVER,
    MATCH_OVER
};

struct MatchInfo {
    int totalRounds = 5;    
    int currentRound = 1;
    int p0Wins = 0;
    int p1Wins = 0;
    GameState state = ROUND_ACTIVE;
    float roundOverTimer = 0.0f;
    std::vector<std::string> mapFiles;
};


GameMap loadMapFromFile(const std::string& path) {
    GameMap map;
    FILE* file = fopen(path.c_str(), "r");
    if (!file) {
        TraceLog(LOG_ERROR, "Failed to open map file: %s", path.c_str());
        return map;
    }
    int width, height;
    if (fscanf(file, "%d %d\n", &width, &height) != 2) {
        TraceLog(LOG_ERROR, "Invalid map file header: %s", path.c_str());
        fclose(file);
        return map;
    }
    map.resize(height, std::vector<Tile>(width, VOID));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int c = fgetc(file);
            if (c == EOF || c == '\n' || c == '\r') {
                x--; 
                continue;
            }

            switch (c) {
                case '#': map[y][x] = TILE; break;
                default:  map[y][x] = VOID; break;
            }
        }
    }
    fclose(file);
    return map;
}

void loadNextMap(MatchInfo &match, GameMap &map) {
    int index = (match.currentRound - 1) % match.mapFiles.size();
    map = loadMapFromFile(match.mapFiles[index]);
}

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

Vector2 LerpVec2(Vector2 a, Vector2 b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    };
}

void renderLevel(GameMap const &map) {
    for (unsigned y = 0; y < map.size(); ++y) {
        for (unsigned x = 0; x < map[y].size(); ++x) {
            if (map[y][x] == TILE) {
                DrawTexture(
                    woodBoxTex,
                    x * TILE_SIZE,
                    y * TILE_SIZE,
                    WHITE
                );
            }
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
			
	float shoulderY = player.y + player.h * 0.50;  
	float shoulderX = (player.facing == 1) 
    ? player.x + player.w * 0.0f   
    : player.x + player.w * 1.0f;  
	
	float armThickness = player.w * 0.25;        
	float armLength    = player.h * 0.45f;       
	
	if (!player.gun) {
	    Rectangle arm = {
	        shoulderX - armThickness * 0.5f,  
	        shoulderY,
	        armThickness,
	        armLength
	    };
	    DrawRectangleRec(arm, WHITE);
	} else {
    Gun const *gun = player.gun;

    float gunScale = player.h / 100.0f; 
    float gunW = gun->w * gunScale;
    float gunH = gun->h * gunScale;

    float armX, armY;
    if (player.facing == 1) {
        armX = shoulderX;
        armY = shoulderY - armThickness * 0.5f;

        Rectangle arm = {armX, armY, armLength, armThickness};
        DrawRectangleRec(arm, WHITE);

        float gunX = arm.x + arm.width;
        float gunY = arm.y + arm.height/2.0f - gunH/2.0f;
        Rectangle gunRect = {gunX, gunY, gunW, gunH};
        DrawRectangleRec(gunRect, BLACK);

    } else {
        armX = shoulderX - armLength;
        armY = shoulderY - armThickness * 0.5f;

        Rectangle arm = {armX, armY, armLength, armThickness};
        DrawRectangleRec(arm, WHITE);

        float gunX = arm.x - gunW;
        float gunY = arm.y + arm.height/2.0f - gunH/2.0f;
        Rectangle gunRect = {gunX, gunY, gunW, gunH};
        DrawRectangleRec(gunRect, BLACK);
    }
	}
	if (player.hitTimer > 0.0f) {
	    DrawRectangle(player.x, player.y, player.w, player.h, Fade(RED, 0.5f));
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

void handlePlayerCollision(Player &player, GameMap const &currentMap, float const dt) {
  float move_x = player.dx * dt;
  player.x += move_x;
  if (hasMapCollision(currentMap, player)) {
    player.x -= move_x;
    player.dx = 0.0f;
  }
  float move_y = player.dy * dt;
  player.y += move_y;
  if (hasMapCollision(currentMap, player)) {
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


Vector2 findValidSpawn(const GameMap &map, float playerW, float playerH) {
    int rows = (int)map.size();
    int cols = rows > 0 ? (int)map[0].size() : 0;
    if (rows == 0 || cols == 0) return {0, 0};

    std::vector<Vector2> candidates;
    int playerTilesWide = (int)ceil(playerW / TILE_SIZE);

    for (int y = rows - 1; y > 0; --y) {
        for (int x = 0; x <= cols - playerTilesWide; ++x) {
            bool floorRun = true;
            for (int i = 0; i < playerTilesWide; ++i) {
                if (map[y][x + i] != TILE || map[y - 1][x + i] == TILE) {
                    floorRun = false;
                    break;
                }
            }
            if (floorRun) {
                candidates.push_back({(float)x, (float)y});
            }
        }
    }
    if (candidates.empty()) {
        TraceLog(LOG_WARNING, "No valid floor found for spawn!");
        return {0, 0};
    }
    for (int attempt = 0; attempt < 1000; ++attempt) {
        Vector2 pick = candidates[GetRandomValue(0, (int)candidates.size() - 1)];
        int tx = (int)pick.x;
        int ty = (int)pick.y;

        float spawnX = tx * TILE_SIZE + (TILE_SIZE * playerTilesWide - playerW) * 0.5f;
        float spawnY = (ty - 4) * TILE_SIZE; 

        Player test{};
        test.x = spawnX;
        test.w = playerW;
        test.h = playerH;

        for (int yDrop = 0; yDrop < TILE_SIZE * 6; yDrop++) {
            test.y = spawnY + yDrop;
            if (hasMapCollision(map, test)) {
                test.y -= 2; 
                break;
            }
        }
        if (!hasMapCollision(map, test)) {
            test.y -= 1; 
            if (!hasMapCollision(map, test))
                return {test.x, test.y};
        }
    }
    TraceLog(LOG_WARNING, "No valid spawn found after 1000 attempts!");
    return {0, 0};
}


void handlePlayerInput(Player &player, float dt, GameMap& currentMap) {
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

    if (!hasMapCollision(currentMap, test)) {
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
		float jump_factor = hasFlag(player.status_flags, GROUNDED) ? 0.0f : 2.5f;
		float spread_angle = gun->spread * (1.0f + speed_factor + jump_factor);
		float angle = baseAngle + ((rand() / (float)RAND_MAX) - 0.5f) * spread_angle;
    
		float vx = cosf(angle) * gun->projectile_speed;
    float vy = sinf(angle) * gun->projectile_speed;
	
		float shoulderY = player.y + player.h * 0.30f;
		float shoulderX = (player.facing == 1) 
		    ? player.x + player.w   
		    : player.x;             
		
		float projX = shoulderX;
		float projY = shoulderY;
		
		if (player.facing == 1) {
		    projX += 5.0f;  
		} else {
		    projX -= 5.0f;  
		}
		
		projectiles.push_back({
		    projX,
		    projY,
		    vx,
		    vy,
		    0.0f,
		    gun->range,
				player.id
		});
  }
}


void handleGrenadeThrow(Player &player, std::vector<Grenade> &grenades) {
    if (isActionPressed(player.controls, player.controls.grenade) && player.grenadeCount > 0) {
        player.grenadeCount--; // Consume one grenade
        
        Grenade g;
        g.radius = 12.0f;
        g.fuse = 2.5f;
        g.bounce = 0.8f;
        g.trail.clear();
        g.exploded = false;

        float throwSpeed = 700.0f;
        float throwAngle = (player.facing == 1) ? -M_PI / 5.0f : M_PI + M_PI / 5.0f; 
        g.dx = cosf(throwAngle) * throwSpeed + player.dx * 0.5f; 
        g.dy = sinf(throwAngle) * throwSpeed + player.dy * 0.5f;

        g.x = player.x + player.w / 2 + (player.facing * 40.0f);
        g.y = player.y + player.h * 0.4f;

        grenades.push_back(g);
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
                       GameMap const &map, std::vector<Player> &players) {
  for (size_t i = 0; i < projectiles.size();) {
    Projectile &p = projectiles[i];
    float move_x = p.dx * dt;
    float move_y = p.dy * dt;
    p.x += move_x;
    p.y += move_y;
		p.trail.push_back({p.x, p.y});
		int const max_trail = 30;
		if (p.trail.size() > max_trail) { 
		    p.trail.erase(p.trail.begin());
		}
    p.traveled += sqrtf(move_x * move_x + move_y * move_y);

    Rectangle rect = {p.x, p.y, 8, 8};

    bool remove = false;

    if (p.traveled >= p.max_distance || hasMapCollision(map, *(Player *)&rect)) {
      remove = true;
    } else {
      for (auto &pl : players) {
        if (!(hasFlag(pl.status_flags, ALIVE))) continue;

        Rectangle prect = {pl.x, pl.y, pl.w, pl.h};
        if (CheckCollisionRecs(rect, prect)) {
          pl.health -= 25;
					pl.hitTimer = 0.2f;
          if (pl.health <= 0) {
						clearFlag(pl.status_flags, ALIVE);
						pl.respawnTimer = 3.0f;
						players[p.ownerId].kills++;
					}
          remove = true;
          break;
        }
      }
    }
    if (remove) {
      projectiles[i] = projectiles.back();
      projectiles.pop_back();
    } else {
      i++;
    }
  }
}


void updateGrenades(std::vector<Grenade> &grenades, float dt,
                    const GameMap &map, std::vector<Player> &players,
                    std::vector<Projectile> &projectiles) {
    const float gravity = 1500.0f;
    const float EPS = 0.1f;       
    const float FLOOR_EPS = 2.0f; 
    const float MIN_BOUNCE_SPEED = 60.0f;

    for (size_t i = 0; i < grenades.size();) {
        Grenade &g = grenades[i];

        g.trail.push_back({g.x, g.y});
        if (g.trail.size() > 25) g.trail.erase(g.trail.begin());

        g.fuse -= dt;
        if (g.fuse <= 0.0f && !g.exploded) {
            g.exploded = true;

            int numProjectiles = 16;
            float speed = 600.0f;
            for (int j = 0; j < numProjectiles; ++j) {
                float angle = j * (2 * M_PI / numProjectiles);
                projectiles.push_back({
                    g.x, g.y,
                    cosf(angle) * speed,
                    sinf(angle) * speed,
                    0.0f,
                    400.0f,
                    -1
                });
            }
        }
        if (g.exploded) {
            grenades[i] = grenades.back();
            grenades.pop_back();
            continue;
        }
        g.dy += gravity * dt;

        float nextX = g.x + g.dx * dt;
        float nextY = g.y + g.dy * dt;

        Rectangle nextRect = {nextX - g.radius, nextY - g.radius, g.radius * 2, g.radius * 2};

        bool grounded = false;

        for (int y = 0; y < (int)map.size(); ++y) {
            for (int x = 0; x < (int)map[y].size(); ++x) {
                if (map[y][x] != TILE) continue;
                Rectangle tile = {(float)x * TILE_SIZE, (float)y * TILE_SIZE,
                                  (float)TILE_SIZE, (float)TILE_SIZE};

                if (CheckCollisionRecs(nextRect, tile)) {
                    if (g.dy > 0 && g.y + g.radius <= tile.y + FLOOR_EPS) {
                        grounded = true;
                        nextY = tile.y - g.radius;
                        g.dy *= -g.bounce;

                        if (fabs(g.dy) < MIN_BOUNCE_SPEED) g.dy = 0;
                    }
                    else if (g.dy < 0 && g.y - g.radius >= tile.y + TILE_SIZE - FLOOR_EPS) {
                        nextY = tile.y + TILE_SIZE + g.radius;
                        g.dy *= -g.bounce;
                    }
                }
            }
        }

        Rectangle horizRect = {nextX - g.radius, g.y - g.radius, g.radius * 2, g.radius * 2};
        for (int y = 0; y < (int)map.size(); ++y) {
            for (int x = 0; x < (int)map[y].size(); ++x) {
                if (map[y][x] != TILE) continue;
                Rectangle tile = {(float)x * TILE_SIZE, (float)y * TILE_SIZE,
                                  (float)TILE_SIZE, (float)TILE_SIZE};

                if (CheckCollisionRecs(horizRect, tile)) {
                    if (g.dx > 0 && g.x + g.radius <= tile.x + EPS) {
                        nextX = tile.x - g.radius;
                        g.dx *= -g.bounce;
                    } else if (g.dx < 0 && g.x - g.radius >= tile.x + TILE_SIZE - EPS) {
                        nextX = tile.x + TILE_SIZE + g.radius;
                        g.dx *= -g.bounce;
                    }
                }
            }
        }
        g.x = nextX;
        g.y = nextY;
        g.dx *= 0.98f;

        if (grounded && fabs(g.dy) < 0.1f && fabs(g.dx) < 5.0f)
      	{
						g.dy = g.dx = 0;
				}

				for (auto &pl : players) {
    			if (!hasFlag(pl.status_flags, ALIVE)) continue;

    			Rectangle prect = {pl.x, pl.y, pl.w, pl.h};

    			if (CheckCollisionCircleRec({g.x, g.y}, g.radius, prect)) {
    			    float closestX = std::clamp(g.x, prect.x, prect.x + prect.width);
    			    float closestY = std::clamp(g.y, prect.y, prect.y + prect.height);

    			    float dx = g.x - closestX;
    			    float dy = g.y - closestY;
    			    float dist2 = dx * dx + dy * dy;

    			    if (dist2 < g.radius * g.radius && dist2 > 0.0001f) {
    			        float dist = sqrtf(dist2);
    			        float overlap = g.radius - dist;

    			        dx /= dist;
    			        dy /= dist;

    			        g.x += dx * overlap;
    			        g.y += dy * overlap;

    			        float vn = g.dx * dx + g.dy * dy;
    			        if (vn < 0) { 
    			            g.dx -= (1 + g.bounce) * vn * dx;
    			            g.dy -= (1 + g.bounce) * vn * dy;
    			        }
    			        g.dx += pl.dx * 0.2f;
    			        g.dy += pl.dy * 0.2f;
    			        g.dx *= 0.98f;
    			        g.dy *= 0.98f;
    			    }
    				}
					}
        ++i;
    }
}


void TryInteract(Player &player, std::vector<Pickup> &pickups, std::vector<Gun> &guns)
{
    if (!player.canInteract || player.nearbyPickupIndex == -1) return;

    Pickup &p = pickups[player.nearbyPickupIndex];
    if (!p.active) return;

    switch (p.type)
    {
        case GUN:
        {
            // If player already has a gun, drop it first
            if (player.gun)
            {
                // Create pickup for current gun
                Pickup dropped;
                dropped.type = GUN;
                dropped.position = { player.x + player.w/2, player.y + player.h/2 };
                dropped.active = true;
                dropped.gunId = (int)(player.gun - &guns[0]);
                dropped.w = player.gun->w;
                dropped.h = player.gun->h;
                
                pickups.push_back(dropped);
                player.gun->picked_up = false;
            }

            // Pick up the new gun
            if (p.gunId >= 0 && p.gunId < (int)guns.size())
            {
                Gun &newGun = guns[p.gunId];
                newGun.picked_up = true;
                player.gun = &newGun;
                p.active = false;
            }
            break;
        }

        case GRENADE:
        {
            if (player.grenadeCount < player.maxGrenades)
            {
                player.grenadeCount = std::min(
                    player.grenadeCount + p.grenadeAmount,
                    player.maxGrenades
                );
                p.active = false;
            }
            break;
        }
    }
    
    // Reset interaction state
    player.nearbyPickupIndex = -1;
    player.canInteract = false;
}


void SpawnPickup(std::vector<Pickup> &pickups, Vector2 pos, PickupType type, int gunId = -1)
{
    Pickup p;
    p.type = type;
    p.position = pos;
    p.active = true;
    p.gunId = gunId;
    pickups.push_back(p);
}


void SpawnGunWithPickup(std::vector<Gun> &guns, std::vector<Pickup> &pickups, const GameMap &map) {
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
        int x = GetRandomValue(0, RES_W - gun.w);
        int y = GetRandomValue(0, RES_H - gun.h);

        Rectangle rect = {(float)x, (float)y, gun.w, gun.h};
        bool collision = false;

        int rows = (int)map.size();
        int cols = map.empty() ? 0 : (int)map[0].size();

        int left = std::max(0, (int)std::floor(rect.x / TILE_SIZE));
        int right = std::min(cols - 1, (int)std::floor((rect.x + rect.width) / TILE_SIZE));
        int top = std::max(0, (int)std::floor(rect.y / TILE_SIZE));
        int bottom = std::min(rows - 1, (int)std::floor((rect.y + rect.height) / TILE_SIZE));

        for (int yy = top; yy <= bottom && !collision; ++yy) {
            for (int xx = left; xx <= right && !collision; ++xx) {
                if (map[yy][xx] == TILE) {
                    Rectangle tile = {(float)xx * TILE_SIZE, (float)yy * TILE_SIZE, TILE_SIZE, TILE_SIZE};
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
    guns.push_back(gun);

		Pickup p;
		p.type = GUN;
		p.position = {gun.x + gun.w / 2, gun.y + gun.h / 2};
		p.active = true;
		p.gunId = (int)(guns.size() - 1);
		
		p.w = gun.w;
		p.h = gun.h;
		
		pickups.push_back(p);
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
	    for (size_t i = 0; i < p.trail.size(); i++) {
	        float alpha = (i + 1) / (float)p.trail.size();
	        DrawCircleV(p.trail[i], 3, Fade(YELLOW, alpha));
	    }
	    DrawCircleV({p.x, p.y}, 4, ORANGE);
	}
}


void renderGrenades(std::vector<Grenade> const &grenades) {
    for (auto const &g : grenades) {
        for (size_t i = 0; i < g.trail.size(); i++) {
            float alpha = (i + 1) / (float)g.trail.size();
            DrawCircleV(g.trail[i], 3, Fade(GREEN, alpha * 0.6f));
        }
        DrawCircleV({g.x, g.y}, g.radius, DARKGREEN);
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




Player initPlayer(GameMap &currentMap) {
  Player player = {};
  player.w = 75.0f;
  player.h = 100.0f;
  Vector2 spawn = findValidSpawn(currentMap, player.w, player.h);
	player.x = spawn.x;
	player.y = spawn.y;
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
	
	player.max_health = 100;
	player.health = player.max_health;
  setFlag(player.status_flags, ALIVE);

  return player;
}

void resetPlayer(Player &player, GameMap &currentMap) {
		player.dx = 0.0f;
    player.dy = 0.0f;

    player.w = 75.0f;
    player.h = 100.0f;
    player.original_h = player.h;

		Vector2 spawn = findValidSpawn(currentMap, player.w, player.h);
    player.x = spawn.x;
    player.y = spawn.y;
    
    player.dash_timer = 0.0f;
    player.slide_timer = 0.0f;
    player.facing = 1;

    player.health = player.max_health;
    player.hitTimer = 0.0f;
    player.respawnTimer = 0.0f;

    player.gun = nullptr;

    player.status_flags = 0;
    setFlag(player.status_flags, GROUNDED);
    setFlag(player.status_flags, ALIVE);
}


void startNewRound(MatchInfo &match, GameMap &map, std::vector<Player> &players) {
    loadNextMap(match, map);

    for (auto &pl : players) {
        resetPlayer(pl, map);
        while (hasMapCollision(map, pl)) {
            pl.x = GetRandomValue(0, RES_W - pl.w);
            pl.y = GetRandomValue(0, RES_H / 2);
        }
    }

    match.state = ROUND_ACTIVE;
}

bool isMatchOver(const MatchInfo &match) {
    int majority = match.totalRounds / 2 + 1;
    return (match.p0Wins >= majority || match.p1Wins >= majority);
}

void init_resources(){
	testDudeTex = LoadTexture("resources/dude75x100.png");
	testBoxBunny = LoadTexture("resources/boxRabbit40x100.png");
	woodBoxTex = LoadTexture("resources/woodBox64x64.png");
}


int main() {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(1080, 720, "Game");
  SetTargetFPS(60);
  HideCursor();
	
	init_resources();
	
	GameMap currentMap;
	MatchInfo match;
	if (match.mapFiles.empty()) {
	    match.mapFiles = {
	        "resources/maps/test.map",
	        "resources/maps/test2.map",
	        "resources/maps/test3.map"
	    };
	    loadNextMap(match, currentMap);
	}

	Camera2D camera = {0};
	camera.target = {RES_W/2.0f, RES_H/2.0f};
	camera.offset = {(float)RES_W/2, (float)RES_H/2}; 
	camera.zoom = 1.0f;
  
	RenderTexture2D renderTarget = LoadRenderTexture(RES_W, RES_H);
	
	std::vector<Grenade> grenades;
  std::vector<Projectile> projectiles;
  std::vector<Gun> guns;
	std::vector<Pickup> pickups;
	float gunSpawnTimer = 0.0f;
  
	SpawnPickup(pickups, {300, 200}, GRENADE);
	SpawnPickup(pickups, {600, 250}, GRENADE);

  Player player0 = initPlayer(currentMap);
  player0.controls = {
      -1,             
      KEY_A, KEY_D,   
      KEY_W, KEY_S,   
      KEY_SPACE,      
      KEY_LEFT_SHIFT, 
      KEY_J,
			KEY_K,
			KEY_E
  };
  Player player1 = initPlayer(currentMap);
  player1.x = 500.0f;  
  player1.controls = {
      0,  
      GAMEPAD_BUTTON_LEFT_FACE_LEFT,
      GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
      GAMEPAD_BUTTON_LEFT_FACE_UP,
      GAMEPAD_BUTTON_LEFT_FACE_DOWN,
      GAMEPAD_BUTTON_RIGHT_FACE_DOWN,  
      GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, 
      GAMEPAD_BUTTON_RIGHT_TRIGGER_1,   
      GAMEPAD_BUTTON_RIGHT_TRIGGER_2,   
      GAMEPAD_BUTTON_LEFT_TRIGGER_2   
  };
	std::vector<Player> players{player0, player1};
	player0.id = 0;
	player1.id = 1;
	
	while (!WindowShouldClose()) {
    float dt = GetFrameTime();
			for (Player &player: players) {
			    handlePlayerInput(player, dt, currentMap);
			    handlePlayerCollision(player, currentMap, dt);
    	player.canInteract = false;
    	player.nearbyPickupIndex = -1;

    	Rectangle playerRect = { player.x, player.y, player.w, player.h };
    	
    	for (int i = 0; i < pickups.size(); i++) {
    	    if (!pickups[i].active) continue;

    	    Rectangle pickupRect = { 
    	        pickups[i].position.x - pickups[i].w / 2.0f, 
    	        pickups[i].position.y - pickups[i].h / 2.0f, 
    	        (float)pickups[i].w, 
    	        (float)pickups[i].h 
    	    };

    	    if (CheckCollisionRecs(playerRect, pickupRect)) {
    	        player.canInteract = true;
    	        player.nearbyPickupIndex = i;
    	        
    	        if (pickups[i].type == GRENADE) {
    	            TryInteract(player, pickups, guns);
    	        }
    	        break; 
    	    }
    	}
    	if (isActionPressed(player.controls, player.controls.interact) && player.canInteract) {
    	    TryInteract(player, pickups, guns);
    	}
    	handleShooting(player, projectiles, dt);
    	if (player.grenadeCount > 0) {
    	    handleGrenadeThrow(player, grenades);
    	}
		}
		updateGrenades(grenades, dt, currentMap, players, projectiles);
		updateProjectiles(projectiles, dt, currentMap, players);

    Vector2 avgPos = {0, 0};
    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    int aliveCount = 0;

    for (auto &pl : players) {
        if (!hasFlag(pl.status_flags, ALIVE)) continue;
        aliveCount++;
        avgPos.x += pl.x + pl.w * 0.5f;
        avgPos.y += pl.y + pl.h * 0.5f;
        minX = std::min(minX, pl.x);
        minY = std::min(minY, pl.y);
        maxX = std::max(maxX, pl.x + pl.w);
        maxY = std::max(maxY, pl.y + pl.h);
    }
    if (aliveCount > 0) {
        avgPos.x /= (float)aliveCount;
        avgPos.y /= (float)aliveCount;

        camera.target = LerpVec2(camera.target, avgPos, 0.10f);

        float pad = 200.0f;
        float viewW = (maxX - minX) + pad;
        float viewH = (maxY - minY) + pad;

        viewW = std::max(viewW, 1.0f);
        viewH = std::max(viewH, 1.0f);

        float zoomX = (float)RES_W / viewW;
        float zoomY = (float)RES_H / viewH;
				
				float const zoom_factor = 0.50f;
        float desiredZoom = std::min(zoomX, zoomY) * zoom_factor;
        desiredZoom = std::clamp(desiredZoom, 0.25f, 2.0f); 

        camera.zoom = camera.zoom + (desiredZoom - camera.zoom) * 0.05f;
    }
	
		float mapWidth  = currentMap[0].size() * TILE_SIZE;
		float mapHeight = currentMap.size() * TILE_SIZE;
		int const falloffBuffer = 1000;
		for (auto &pl : players) {
		    if (hasFlag(pl.status_flags, ALIVE)) {
		        bool fellBelow = (pl.y > mapHeight + falloffBuffer);
		        bool fellLeft = (pl.x + pl.w < -falloffBuffer); 
		
		        if (fellBelow || fellLeft) {
		            clearFlag(pl.status_flags, ALIVE);
		            pl.respawnTimer = 0.0f;
		            pl.health = 0.0f;
		        }
		    }
		}
		
		gunSpawnTimer -= dt;
		if (gunSpawnTimer <= 0.0f) {
		    SpawnGunWithPickup(guns, pickups, currentMap);
		    gunSpawnTimer = 10.0f; 
		}

		for (Player &pl : players) {
		    if (pl.hitTimer > 0.0f) pl.hitTimer -= dt;
		}
		
		aliveCount = 0;
		int alivePlayerId = -1;
		for (int i = 0; i < players.size(); i++) {
		    if (hasFlag(players[i].status_flags, ALIVE)) {
		        aliveCount++;
		        alivePlayerId = i;
		    }
		}
		
		if (match.state == ROUND_ACTIVE) {
		    if (aliveCount <= 1) {
		        match.state = ROUND_OVER;
		        match.roundOverTimer = 3.0f;
		
		        if (alivePlayerId == 0) match.p0Wins++;
		        if (alivePlayerId == 1) match.p1Wins++;
		    }
		}
		
		else if (match.state == ROUND_OVER) {
		    match.roundOverTimer -= dt;
		    if (match.roundOverTimer <= 0.0f) {
		        match.currentRound++;
		        if (isMatchOver(match)) {
		            match.state = MATCH_OVER;
		        } else {
		            startNewRound(match, currentMap, players);
		        }
		    }
		}
		
		else if (match.state == MATCH_OVER) {
		    if (IsKeyPressed(KEY_R)) {
		        match = MatchInfo(); 
		        match.mapFiles = {
		            "resources/maps/test.map",
		            "resources/maps/test2.map",
		            "resources/maps/test3.map"
		        };
		        startNewRound(match, currentMap, players);
		    }
		}

    BeginTextureMode(renderTarget);
    ClearBackground(SKYBLUE);

    BeginDrawing();
    BeginMode2D(camera);

    renderLevel(currentMap);
    for (Player &player: players) {
        if (!hasFlag(player.status_flags, ALIVE)) continue;
        renderPlayer(player);
    }
    renderGuns(guns);

		for (auto &p : pickups) {
		    if (!p.active) continue;
		
		    Color c = (p.type == GUN) ? ORANGE : SKYBLUE;
		    DrawCircleV(p.position, 8, c);
		}
    renderProjectiles(projectiles);
		renderGrenades(grenades);

    EndMode2D();
    EndDrawing();
    EndTextureMode();

		DrawText(TextFormat("Round %d / %d", match.currentRound, match.totalRounds), 20, 20, 30, WHITE);
		DrawText(TextFormat("P1 Wins: %d  P2 Wins: %d", match.p0Wins, match.p1Wins), 20, 60, 30, WHITE);
		
		if (match.state == ROUND_OVER) {
		    DrawText("Round Over!", RES_W/2 - 150, RES_H/2 - 40, 60, RED);
		}
		if (match.state == MATCH_OVER) {
		    const char *winner =
		        (match.p0Wins > match.p1Wins) ? "PLAYER 1 WINS" : "PLAYER 2 WINS";
		    DrawText(winner, RES_W/2 - 200, RES_H/2 - 40, 60, YELLOW);
		    DrawText("Press R to Restart", RES_W/2 - 180, RES_H/2 + 40, 30, WHITE);
		}
    renderToScreen(renderTarget);
  }
  UnloadRenderTexture(renderTarget);
  CloseWindow();
}
