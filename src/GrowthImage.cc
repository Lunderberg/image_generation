#include "GrowthImage.hh"

#include <iostream>
using std::cout;
using std::endl;

#include <cassert>
#include <cfloat>
#include <algorithm>
#include <ctime>

#include "common.hh"

GrowthImage::GrowthImage(int width, int height, int seed)
	: image(width,height), view(boost::gil::view(image)), previous_loc(-1,-1),
		color_choice(ColorChoice::Nearest),	preferred_location_iterations(10){
	rng = std::mt19937(seed ? seed : time(0));
	Reset();
	palette.GenerateUniformPalette(width*height);
}

void GrowthImage::Seed(int seed){
	rng = std::mt19937(seed);
}

int GrowthImage::GetWidth(){
	return image.width();
}

int GrowthImage::GetHeight(){
	return image.height();
}

void GrowthImage::SetColorChoice(ColorChoice c){
	color_choice = c;
}

void GrowthImage::SetLocationChoice(LocationChoice c){
	location_choice = c;
}

void GrowthImage::SetPreferredLocationIterations(int n){
	preferred_location_iterations = n;
}

void GrowthImage::Reset(){
	filled.clear();
	filled.resize(image.width());
	for(int i=0; i<image.width(); i++){
		filled[i].resize(image.height());
		for(int j=0; j<image.height(); j++){
			filled[i][j] = false;
		}
	}

	frontier.clear();

	FirstIteration();
}

void GrowthImage::FirstIteration(){
	frontier.push_back({randint(rng,image.width()),
				              randint(rng,image.height())});
}

bool GrowthImage::Iterate(){
	auto loc = ChooseLocation();
	auto color = ChooseColor(loc);
	view(loc.i,loc.j) = {color.r, color.g, color.b};

	// Extend the frontier
	filled[loc.i][loc.j] = true;
	for(int di=-1; di<=1; di++){
		for(int dj=-1; dj<=1; dj++){
			Point p(loc.i+di, loc.j+dj);
			if(p.i>=0 && p.i<image.width() &&
				 p.j>=0 && p.j<image.height() &&
				 !is_in(frontier,p) &&
				 !filled[p.i][p.j]){
				p.preference = ChoosePreference(p,color);
				frontier.push_back(p);
			}
		}
	}

	previous_loc = loc;

	return frontier.size();
}

void GrowthImage::IterateUntilDone(){
	int body_size = 0;
	while(frontier.size()){
		if(body_size%1000==0){
			cout << "\r                                                   \r"
					 << "Body: " << body_size << "\tFrontier: " << frontier.size()
					 << "\tUnexplored: " << image.height()*image.width() - body_size - frontier.size()
					 << std::flush;
		}
		Iterate();
		body_size++;
	}
	cout << endl;
}

Point GrowthImage::ChooseLocation(){
	switch(location_choice){
	case LocationChoice::Random:
		return ChooseFrontierLocation();
	case LocationChoice::Snaking:
		return ChooseSnakingLocation();
	case LocationChoice::Preferred:
		return ChoosePreferredLocation(preferred_location_iterations);
	}
}

Point GrowthImage::ChooseFrontierLocation(){
	return poprandom(rng,frontier);
}

Point GrowthImage::ChoosePreferredLocation(int n_check){
	assert(n_check>0);
	int best_index;
	double best_preference = -DBL_MAX;
	for(int i=0; i<n_check; i++){
		int index = randint(rng,frontier.size());
		if(frontier[index].preference > best_preference){
			best_preference = frontier[index].preference;
			best_index = index;
		}
	}
	return popanywhere(frontier,best_index);
}

double GrowthImage::ChoosePreference(Point p, Color){
	if(goal_loc.i == -1 || filled[goal_loc.i][goal_loc.j]){
		goal_loc = {randint(rng,image.width()),
								randint(rng,image.height())};
	}

	double di = p.i - goal_loc.i;
	double dj = p.j - goal_loc.j;
	return -(di*di + dj*dj);
}

Point GrowthImage::ChooseSnakingLocation(){
	std::vector<Point> free_locs;
	for(int i=0; i<4; i++){
		int di = (i%2)*2 - 1;
		int dj = (i/2)*2 - 1;
		Point new_loc = Point(previous_loc.i+di,previous_loc.j+dj);
		if(new_loc.i>=0 && new_loc.i<image.width() &&
			 new_loc.j>=0 && new_loc.j<image.height() &&
			 !filled[new_loc.i][new_loc.j]){
			free_locs.push_back(new_loc);
		}
	}
	if(free_locs.size()){
		Point next_loc = free_locs[randint(rng,free_locs.size())];
		frontier.erase(std::remove_if(frontier.begin(),frontier.end(),
																	[&next_loc](const Point& o){return o==next_loc;}),
									 frontier.end());
		return next_loc;
	} else {
		return poprandom(rng,frontier);
	}
}

Color GrowthImage::ChooseColor(Point loc){
	switch(color_choice){
	case ColorChoice::Nearest:
		return ChooseNearestColor(loc);
	case ColorChoice::Sequential:
		return ChooseSequentialColor(loc);
	}
}

Color GrowthImage::ChooseSequentialColor(Point loc){
	return palette.PopBack();
}

Color GrowthImage::ChooseNearestColor(Point loc){
	// Find the average surrounding color.
	double ave_r = 0;
	double ave_g = 0;
	double ave_b = 0;
	int count = 0;
	for(int di=-1; di<=1; di++){
		for(int dj=-1; dj<=1; dj++){
			Point p(loc.i+di,loc.j+dj);
			if(p.i>=0 && p.i<image.width() &&
				 p.j>=0 && p.j<image.height() &&
				 filled[p.i][p.j]){
				ave_r += view(p.i,p.j)[0];
				ave_g += view(p.i,p.j)[1];
				ave_b += view(p.i,p.j)[2];
				count++;
			}
		}
	}

	if(count){
		// Neighbors exist, find the closest color.
		ave_r /= count;
		ave_g /= count;
		ave_b /= count;
		return palette.PopClosest({ave_r,ave_g,ave_b});

	} else {
		// No neighbors, so take a random color.
		return palette.PopRandom(rng);
	}
}

