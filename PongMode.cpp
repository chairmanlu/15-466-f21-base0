#include "PongMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>

PongMode::PongMode() {

	srand((unsigned int) time(NULL));

	//set up trail as if ball has been here for 'forever':
	ball_trail.clear();
	ball_trail.emplace_back(ball, trail_length);
	ball_trail.emplace_back(ball, 0.0f);

	
	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of PongMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

PongMode::~PongMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

bool PongMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_MOUSEMOTION) {
		//convert mouse from window pixels (top-left origin, +y is down) to clip space ([-1,1]x[-1,1], +y is up):
		glm::vec2 clip_mouse = glm::vec2(
			(evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
			(evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f
		);
		cursor_pos = clip_to_court * glm::vec3(clip_mouse, 1.0f);

		left_paddle.y = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).y;
	}
	if (evt.type == SDL_MOUSEBUTTONUP){
		if(cursor_mode != CURSOR_NORMAL && !overlaps_buildings(cursor_pos,building_radius) && in_base(cursor_pos,building_radius)){
			switch(cursor_mode){
				case BUILDING_SHOOTER:
					if(cursor_mode == BUILDING_SHOOTER && left_money >= SHOOTER_PRICE){
						building_cooldowns.push_back(SHOOTER_COOL);
						buildings.push_back(glm::vec2(cursor_pos.x,cursor_pos.y));
						building_types.push_back(cursor_mode);
						left_money -= SHOOTER_PRICE;
					}
					break;
				case BUILDING_WALL:
					if(left_money >= WALL_PRICE){
						buildings.push_back(glm::vec2(cursor_pos.x,cursor_pos.y));
						building_types.push_back(cursor_mode);
						building_cooldowns.push_back(WALL_COOL);
						left_money -= WALL_PRICE;
					}
					break;
				case BUILDING_FARM:
					if (left_money >= FARM_PRICE){
						building_cooldowns.push_back(FARM_COOL);
						buildings.push_back(glm::vec2(cursor_pos.x,cursor_pos.y));
						building_types.push_back(cursor_mode);
						left_money -= FARM_PRICE;
					}
					break;
			}
		}
	}

	if (evt.type == SDL_KEYUP){
		if(evt.key.keysym.sym == SDLK_SPACE){
			cursor_mode = CURSOR_NORMAL;
		}
		else if(evt.key.keysym.sym == SDLK_q){
			cursor_mode = BUILDING_SHOOTER;
		}
		else if(evt.key.keysym.sym == SDLK_w){
			cursor_mode = BUILDING_WALL;
		}
		else if(evt.key.keysym.sym == SDLK_e){
			cursor_mode = BUILDING_FARM;
		}
	}

	return false;
}

void PongMode::update(float elapsed) {

	static std::mt19937 mt; //mersenne twister pseudo-random number generator

	//----- paddle update -----

	{ //right player ai:
		ai_offset_update -= elapsed;
		if (ai_offset_update < elapsed) {
			//update again in [0.5,1.0) seconds:
			ai_offset_update = (mt() / float(mt.max())) * 0.5f + 0.5f;
			ai_offset = (mt() / float(mt.max())) * 2.5f - 1.25f;
		}
		if(ball.x < right_paddle.x){
			if (right_paddle.y < ball.y + ai_offset) {
				right_paddle.y = std::min(ball.y + ai_offset, right_paddle.y + 2.0f * elapsed);
			} else {
				right_paddle.y = std::max(ball.y + ai_offset, right_paddle.y - 2.0f * elapsed);
			}
		}
		//Avoid ball if behind the paddle
		else{
			if (right_paddle.y < ball.y + ai_offset) {
				right_paddle.y = std::min(ball.y + ai_offset, right_paddle.y - 2.0f * elapsed);
			} else {
				right_paddle.y = std::max(ball.y + ai_offset, right_paddle.y + 2.0f * elapsed);
			}
		}

		if(enough_money()){
			int tries = 0;
			while(tries++ < 1000){
				//Random number generation from https://stackoverflow.com/questions/686353/random-float-number-generation
				float x = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
				x = court_radius.x - x * (base_length - 2.0f * building_radius.x - buffer_radius) - building_radius.x;
				float y = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
				y = (2.0f * y - 1.0f) * (court_radius.y - 2.0f * building_radius.y);

				glm::vec2 pos = glm::vec2(x,y);
				if(!overlaps_buildings(pos, building_radius)){
					switch(next_purchase){
						case BUILDING_SHOOTER:
							if(right_money >= SHOOTER_PRICE){
								building_cooldowns.push_back(SHOOTER_COOL);
								buildings.push_back(glm::vec2(pos.x,pos.y));
								building_types.push_back(-1 * BUILDING_SHOOTER);
								right_money -= SHOOTER_PRICE;
							}
							break;
						case BUILDING_WALL:
							if(right_money >= WALL_PRICE){
								buildings.push_back(glm::vec2(pos.x,pos.y));
								building_types.push_back(BUILDING_WALL);
								building_cooldowns.push_back(WALL_COOL);
								right_money -= WALL_PRICE;
							}
							break;
						case BUILDING_FARM:
							if (right_money >= FARM_PRICE){
								building_cooldowns.push_back(FARM_COOL);
								buildings.push_back(glm::vec2(pos.x,pos.y));
								building_types.push_back(-1 * BUILDING_FARM);
								right_money -= FARM_PRICE;
							}
							break;
					}
					
					next_purchase = rand() % 3 + 1;

					break;
				}
			}
		}
	}

	//passive income
	income_cooldown -= elapsed;
	while(income_cooldown < 0){
		income_cooldown += INCOME_COOL;
		left_money++;
		right_money++;
	}

	//clamp paddles to court:
	right_paddle.y = std::max(right_paddle.y, -court_radius.y + paddle_radius.y);
	right_paddle.y = std::min(right_paddle.y,  court_radius.y - paddle_radius.y);

	left_paddle.y = std::max(left_paddle.y, -court_radius.y + paddle_radius.y);
	left_paddle.y = std::min(left_paddle.y,  court_radius.y - paddle_radius.y);

	//----- ball update -----

	//speed of ball increases every second:
	float speed_multiplier = 4.0f * std::pow(2.0f, (left_score + right_score) / 4.0f);

	//velocity cap, though (otherwise ball can pass through paddles):
	speed_multiplier = std::min(speed_multiplier, 10.0f);

	ball += elapsed * speed_multiplier * ball_velocity;

	//---- building cooldowns ----
	for(int i=0;i<buildings.size();i++){
		building_cooldowns[i] -= elapsed;
		while(building_cooldowns[i] < 0){
			switch(abs(building_types[i])){
				case BUILDING_SHOOTER:
					//spawn bullet
					if(building_types[i] == BUILDING_SHOOTER){
						glm::vec2 pos = glm::vec2(buildings[i].x, buildings[i].y);
						pos.x += building_radius.x + 2.0f * bullet_radius.x;
						left_bullets.push_back(pos);
					}
					else{
						glm::vec2 pos = glm::vec2(buildings[i].x, buildings[i].y);
						pos.x -= building_radius.x + 2.0f * bullet_radius.x;
						right_bullets.push_back(pos);
					}
					building_cooldowns[i] += SHOOTER_COOL;
					break;
				case BUILDING_WALL:
					building_cooldowns[i] += WALL_COOL;
					break;
				case BUILDING_FARM:
					//Increase money
					if(building_types[i] == BUILDING_FARM){
						left_money++;
					}
					else{
						right_money++;
					}
					building_cooldowns[i] += FARM_COOL;
					break;
			}
		}
	}

	for(int i=0;i<left_bullets.size();i++){
		left_bullets[i].x += elapsed * bullet_speed;
	}
	for(int i=0;i<right_bullets.size();i++){
		right_bullets[i].x -= elapsed * bullet_speed;
	}


	//---- collision handling ----

	//paddles:
	auto paddle_vs_ball = [this](glm::vec2 const &paddle) {
		//compute area of overlap:
		glm::vec2 min = glm::max(paddle - paddle_radius, ball - ball_radius);
		glm::vec2 max = glm::min(paddle + paddle_radius, ball + ball_radius);

		//if no overlap, no collision:
		if (min.x > max.x || min.y > max.y) return;

		if (max.x - min.x > max.y - min.y) {
			//wider overlap in x => bounce in y direction:
			if (ball.y > paddle.y) {
				ball.y = paddle.y + paddle_radius.y + ball_radius.y;
				ball_velocity.y = std::abs(ball_velocity.y);
			} else {
				ball.y = paddle.y - paddle_radius.y - ball_radius.y;
				ball_velocity.y = -std::abs(ball_velocity.y);
			}
		} else {
			//wider overlap in y => bounce in x direction:
			if (ball.x > paddle.x) {
				ball.x = paddle.x + paddle_radius.x + ball_radius.x;
				ball_velocity.x = std::abs(ball_velocity.x);
			} else {
				ball.x = paddle.x - paddle_radius.x - ball_radius.x;
				ball_velocity.x = -std::abs(ball_velocity.x);
			}
			//warp y velocity based on offset from paddle center:
			float vel = (ball.y - paddle.y) / (paddle_radius.y + ball_radius.y);
			ball_velocity.y = glm::mix(ball_velocity.y, vel, 0.75f);
		}
	};
	paddle_vs_ball(left_paddle);
	paddle_vs_ball(right_paddle);

	for(int i=0;i<buildings.size();i++){
		if(overlaps(ball,ball_radius,buildings[i],building_radius)){
			//Bounce back if hit wall
			if(abs(building_types[i]) == BUILDING_WALL){
				//Collision detection from above
				glm::vec2 min = glm::max(buildings[i] - building_radius, ball - ball_radius);
				glm::vec2 max = glm::min(buildings[i] + building_radius, ball + ball_radius);

				//if no overlap, no collision:
				if (min.x > max.x || min.y > max.y) return;

				if (max.x - min.x > max.y - min.y) {
					//wider overlap in x => bounce in y direction:
					if (ball.y > buildings[i].y) {
						ball.y = buildings[i].y + building_radius.y + ball_radius.y;
						ball_velocity.y = std::abs(ball_velocity.y);
					} else {
						ball.y = buildings[i].y - building_radius.y - ball_radius.y;
						ball_velocity.y = -std::abs(ball_velocity.y);
					}
				} else {
					//wider overlap in y => bounce in x direction:
					if (ball.x > buildings[i].x) {
						ball.x = buildings[i].x + building_radius.x + ball_radius.x;
						ball_velocity.x = std::abs(ball_velocity.x);
					} else {
						ball.x = buildings[i].x - building_radius.x - ball_radius.x;
						ball_velocity.x = -std::abs(ball_velocity.x);
					}
					//warp y velocity based on offset from buildings[i] center:
					float vel = (ball.y - buildings[i].y) / (building_radius.y + ball_radius.y);
					ball_velocity.y = glm::mix(ball_velocity.y, vel, 0.75f);
				}

			}
			buildings.erase(buildings.begin() + i);
			building_types.erase(building_types.begin() + i);
		}
	}


	//court walls:
	if (ball.y > court_radius.y - ball_radius.y) {
		ball.y = court_radius.y - ball_radius.y;
		if (ball_velocity.y > 0.0f) {
			ball_velocity.y = -ball_velocity.y;
		}
	}
	if (ball.y < -court_radius.y + ball_radius.y) {
		ball.y = -court_radius.y + ball_radius.y;
		if (ball_velocity.y < 0.0f) {
			ball_velocity.y = -ball_velocity.y;
		}
	}

	if (ball.x > court_radius.x - ball_radius.x) {
		ball.x = court_radius.x - ball_radius.x;
		if (ball_velocity.x > 0.0f) {
			ball_velocity.x = -ball_velocity.x;
			right_health -= 10;
			if(right_health < 0){
				right_health = 0;
			}
		}
	}
	if (ball.x < -court_radius.x + ball_radius.x) {
		ball.x = -court_radius.x + ball_radius.x;
		if (ball_velocity.x < 0.0f) {
			ball_velocity.x = -ball_velocity.x;
			left_health -= 10;
			if(left_health < 0){
				left_health = 0;
			}
		}
	}

	//Bullet collisions
	
	for(int i=0;i<left_bullets.size();i++){
		//walls from above
		if (left_bullets[i].x > court_radius.x - bullet_radius.x) {
			left_bullets.erase(left_bullets.begin() + i);
			right_health -= 5;
			if(right_health < 0){
				right_health = 0;
			}
		}

		//Paddles
		if(overlaps(left_paddle,paddle_radius,left_bullets[i], bullet_radius) ||
		   overlaps(right_paddle,paddle_radius,left_bullets[i], bullet_radius)){
			left_bullets.erase(left_bullets.begin() + i);
		}

		//Buildings
		for(int j=0;j<buildings.size();j++){
			if(overlaps(buildings[j],building_radius,left_bullets[i],bullet_radius)){
				if(abs(building_types[j]) != BUILDING_WALL){
					buildings.erase(buildings.begin() + j);
					building_types.erase(building_types.begin() + j);

				}
				left_bullets.erase(left_bullets.begin() + i);
			}

		}
	}

	for(int i=0;i<right_bullets.size();i++){
		//walls from above
		if (right_bullets[i].x < -court_radius.x + bullet_radius.x) {
			std::cout << "here\n" << std::endl;
			right_bullets.erase(right_bullets.begin() + i);
			left_health -= 5;
			if(left_health < 0){
				left_health = 0;
			}
		}

		//Paddles
		if(overlaps(left_paddle,paddle_radius,right_bullets[i], bullet_radius) ||
		   overlaps(right_paddle,paddle_radius,right_bullets[i], bullet_radius)){
			right_bullets.erase(right_bullets.begin() + i);
		}

		//Buildings
		for(int j=0;j<buildings.size();j++){
			if(overlaps(buildings[j],building_radius,right_bullets[i],bullet_radius)){
				if(abs(building_types[j]) != BUILDING_WALL){
					buildings.erase(buildings.begin() + j);
					building_types.erase(building_types.begin() + j);

				}
				right_bullets.erase(right_bullets.begin() + i);
			}

		}
	}
	
	
	//----- gradient trails -----

	//age up all locations in ball trail:
	for (auto &t : ball_trail) {
		t.z += elapsed;
	}
	//store fresh location at back of ball trail:
	ball_trail.emplace_back(ball, 0.0f);

	//trim any too-old locations from back of trail:
	//NOTE: since trail drawing interpolates between points, only removes back element if second-to-back element is too old:
	while (ball_trail.size() >= 2 && ball_trail[1].z > trail_length) {
		ball_trail.pop_front();
	}

	if(left_health == 0 || right_health == 0){
		Mode::set_current(std::make_shared< PongMode >());
	}
}

void PongMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x193b59ff);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0xf2d2b6ff);
	const glm::u8vec4 shadow_color = HEX_TO_U8VEC4(0xf2ad94ff);
	const glm::u8vec4 valid_color = HEX_TO_U8VEC4(0x00ff0080);
	const glm::u8vec4 invalid_color = HEX_TO_U8VEC4(0xff000080);
	const glm::u8vec4 money_color = HEX_TO_U8VEC4(0xffee00ff);
	const glm::u8vec4 shooter_color1 = HEX_TO_U8VEC4(0xffffffff);
	const glm::u8vec4 shooter_color2 = HEX_TO_U8VEC4(0x000000ff);
	const glm::u8vec4 wall_color = HEX_TO_U8VEC4(0xffffffff);
	const glm::u8vec4 wall_preview_color = HEX_TO_U8VEC4(0xffffff80);
	const glm::u8vec4 farm_color1 = HEX_TO_U8VEC4(0x0db507ff);
	const glm::u8vec4 farm_color2 = HEX_TO_U8VEC4(0xbd6f17ff);
	const std::vector< glm::u8vec4 > trail_colors = {
		HEX_TO_U8VEC4(0xf2ad9488),
		HEX_TO_U8VEC4(0xf2897288),
		HEX_TO_U8VEC4(0xbacac088),
	};
	#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float shadow_offset = 0.07f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//draw rectangle as two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};


	//shadows for everything (except the trail):

	glm::vec2 s = glm::vec2(0.0f,-shadow_offset);

	draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f)+s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f)+s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius)+s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius)+s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	draw_rectangle(left_paddle+s, paddle_radius, shadow_color);
	draw_rectangle(right_paddle+s, paddle_radius, shadow_color);
	draw_rectangle(ball+s, ball_radius, shadow_color);

	//ball's trail:
	if (ball_trail.size() >= 2) {
		//start ti at second element so there is always something before it to interpolate from:
		std::deque< glm::vec3 >::iterator ti = ball_trail.begin() + 1;
		//draw trail from oldest-to-newest:
		constexpr uint32_t STEPS = 20;
		//draw from [STEPS, ..., 1]:
		for (uint32_t step = STEPS; step > 0; --step) {
			//time at which to draw the trail element:
			float t = step / float(STEPS) * trail_length;
			//advance ti until 'just before' t:
			while (ti != ball_trail.end() && ti->z > t) ++ti;
			//if we ran out of recorded tail, stop drawing:
			if (ti == ball_trail.end()) break;
			//interpolate between previous and current trail point to the correct time:
			glm::vec3 a = *(ti-1);
			glm::vec3 b = *(ti);
			glm::vec2 at = (t - a.z) / (b.z - a.z) * (glm::vec2(b) - glm::vec2(a)) + glm::vec2(a);

			//look up color using linear interpolation:
			//compute (continuous) index:
			float c = (step-1) / float(STEPS-1) * trail_colors.size();
			//split into an integer and fractional portion:
			int32_t ci = int32_t(std::floor(c));
			float cf = c - ci;
			//clamp to allowable range (shouldn't ever be needed but good to think about for general interpolation):
			if (ci < 0) {
				ci = 0;
				cf = 0.0f;
			}
			if (ci > int32_t(trail_colors.size())-2) {
				ci = int32_t(trail_colors.size())-2;
				cf = 1.0f;
			}
			//do the interpolation (casting to floating point vectors because glm::mix doesn't have an overload for u8 vectors):
			glm::u8vec4 color = glm::u8vec4(
				glm::mix(glm::vec4(trail_colors[ci]), glm::vec4(trail_colors[ci+1]), cf)
			);

			//draw:
			draw_rectangle(at, ball_radius, color);
		}
	}

	//solid objects:

	//walls:
	draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);

	//paddles:
	draw_rectangle(left_paddle, paddle_radius, fg_color);
	draw_rectangle(right_paddle, paddle_radius, fg_color);
	

	//ball:
	draw_rectangle(ball, ball_radius, fg_color);

	//buildings:
	
	auto draw_shooter = [&](glm::vec2 pos){
		draw_rectangle(glm::vec2(pos),glm::vec2(building_radius), shooter_color1);
		draw_rectangle(glm::vec2(pos),glm::vec2(building_radius / 2.0f), shooter_color2);
	};

	auto draw_wall = [&](glm::vec2 pos){
		draw_rectangle(glm::vec2(pos),glm::vec2(building_radius), wall_color);
	};

	auto draw_farm = [&](glm::vec2 pos){
		draw_rectangle(glm::vec2(pos),glm::vec2(building_radius), farm_color1);
		draw_rectangle(glm::vec2(pos),glm::vec2(building_radius.x / 4.0f, building_radius.y), farm_color2);
		draw_rectangle(glm::vec2(pos),glm::vec2(building_radius.x, building_radius.y / 4.0f), farm_color2);
	};

	for(int i=0;i<buildings.size();i++){
		switch(abs(building_types[i])){
			case BUILDING_SHOOTER:
				draw_shooter(buildings[i]);
				break;
			case BUILDING_WALL:
				draw_wall(buildings[i]);
				break;
			case BUILDING_FARM:
				draw_farm(buildings[i]);
				break;
		}
	}

	//bullets
	for(int i=0;i<left_bullets.size();i++){
		draw_rectangle(glm::vec2(left_bullets[i]),glm::vec2(bullet_radius), fg_color);
	}
	for(int i=0;i<right_bullets.size();i++){
		draw_rectangle(glm::vec2(right_bullets[i]),glm::vec2(bullet_radius), fg_color);
	}

	//building outline
	switch(cursor_mode){
		case BUILDING_FARM:
			draw_farm(cursor_pos);
			if(overlaps_buildings(cursor_pos,building_radius) || !in_base(cursor_pos, building_radius) || left_money < FARM_PRICE){
				draw_rectangle(cursor_pos,glm::vec2(building_radius), invalid_color);
			}
			else{
				draw_rectangle(cursor_pos,glm::vec2(building_radius), valid_color);
			}
			break;
		case BUILDING_SHOOTER:
			draw_shooter(cursor_pos);
			if(overlaps_buildings(cursor_pos,building_radius) || !in_base(cursor_pos, building_radius) || left_money < SHOOTER_PRICE){
				draw_rectangle(cursor_pos,glm::vec2(building_radius), invalid_color);
			}
			else{
				draw_rectangle(cursor_pos,glm::vec2(building_radius), valid_color);
			}
			break;
			break;
		case BUILDING_WALL:
			draw_wall(cursor_pos);
			if(overlaps_buildings(cursor_pos,building_radius) || !in_base(cursor_pos, building_radius) || left_money < WALL_PRICE){
				draw_rectangle(cursor_pos,glm::vec2(building_radius), invalid_color);
			}
			else{
				draw_rectangle(cursor_pos,glm::vec2(building_radius), valid_color);
			}
			break;
	}

	//health:
	glm::vec2 health_radius = glm::vec2(0.05f, 0.1f);
	draw_rectangle(glm::vec2(-court_radius.x + health_radius.x * left_health / 2.0f, court_radius.y + 2.0f * wall_radius + 2.0f * health_radius.y),
	               glm::vec2(health_radius.x * left_health / 2.0f, health_radius.y), fg_color);
	draw_rectangle(glm::vec2(court_radius.x - health_radius.x * right_health / 2.0f, court_radius.y + 2.0f * wall_radius + 2.0f * health_radius.y),
	               glm::vec2(health_radius.x * right_health / 2.0f, health_radius.y), fg_color);

	//money - adapted from score drawing code
	glm::vec2 money_radius = glm::vec2(0.1f, 0.1f);
	for(uint32_t i = 0; i < left_money; ++i){
		draw_rectangle(glm::vec2( -court_radius.x + (2.0f + 3.0f * i) * money_radius.x, -court_radius.y - 2.0f * wall_radius - 2.0f * money_radius.y), money_radius, money_color);
	}
	for (uint32_t i = 0; i < right_money; ++i) {
		draw_rectangle(glm::vec2( court_radius.x - (2.0f + 3.0f * i) * money_radius.x, -court_radius.y - 2.0f * wall_radius - 2.0f * money_radius.y), money_radius, money_color);
	}

	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - 2.0f * money_radius.y - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f * health_radius.y + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero so things will be drawn just with their colors:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.

}
