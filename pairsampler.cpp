#include "pairsampler.h"

#include <algorithm>
#include <cstdio>
#include <cassert>

#include "negtrain.h"
#include "mathutils.h"

PairSampler::PairSampler(const char *adj_list_file_name)
{
	printf("loading %s ...\n", adj_list_file_name);
	FILE *fp = fopen(adj_list_file_name, "rb");
	assert(fp != 0);

	fread(&num_vertex_left_, sizeof(int), 1, fp);
	fread(&num_vertex_right_, sizeof(int), 1, fp);
	printf("left: %d right: %d\n", num_vertex_left_, num_vertex_right_);

	//right_vertex_dists_ = new std::discrete_distribution<int>[num_vertex_left_];
	right_vertex_samplers_ = new MultinomialSampler[num_vertex_left_];

	int *left_weights = new int[num_vertex_left_];
	int *right_weights = new int[num_vertex_right_];
	std::fill(left_weights, left_weights + num_vertex_left_, 0);
	std::fill(right_weights, right_weights + num_vertex_right_, 0);

	adj_list_ = new int*[num_vertex_left_];
	num_adj_vertices_ = new int[num_vertex_left_];

	cnts_ = new int*[num_vertex_left_];

	for (int i = 0; i < num_vertex_left_; ++i)
	{
		fread(&num_adj_vertices_[i], sizeof(int), 1, fp);

		adj_list_[i] = new int[num_adj_vertices_[i]];
		fread(adj_list_[i], sizeof(int), num_adj_vertices_[i], fp);

		cnts_[i] = new int[num_adj_vertices_[i]];
		std::fill(cnts_[i], cnts_[i] + num_adj_vertices_[i], 0);

		unsigned short *weights = new unsigned short[num_adj_vertices_[i]];
		fread(weights, sizeof(unsigned short), num_adj_vertices_[i], fp);
		for (int j = 0; j < num_adj_vertices_[i]; ++j)
		{
			left_weights[i] += weights[j];
			right_weights[adj_list_[i][j]] += weights[j];
			sum_weights_ += weights[j];
		}

		//right_vertex_dists_[i] = std::discrete_distribution<int>(weights, weights + num_adj_vertices_[i]);
		right_vertex_samplers_[i].Init(weights, num_adj_vertices_[i]);
		delete[] weights;

		if (i % 100000 == 100000 - 1)
			printf("%d\n", i + 1);
	}

	fclose(fp);

	left_vertex_dist_ = std::discrete_distribution<int>(left_weights,
		left_weights + num_vertex_left_);
	//printf("num: %d\n", num_vertex_left_);
	//left_vertex_sampler_.Init(left_weights, num_vertex_left_);

	float *neg_sampling_weights = NegTrain::GetDefNegativeSamplingWeights(right_weights,
		num_vertex_right_);
	neg_sampling_dist_ = std::discrete_distribution<int>(neg_sampling_weights,
		neg_sampling_weights + num_vertex_right_);
	delete[] neg_sampling_weights;

	delete[] left_weights;
	delete[] right_weights;

	printf("done.\n");
}

PairSampler::~PairSampler()
{
	if (right_vertex_dists_ != 0)
		delete[] right_vertex_dists_;
	if (right_vertex_samplers_ != 0)
		delete[] right_vertex_samplers_;
}

void PairSampler::SamplePair(int &lidx, int &ridx, std::default_random_engine &generator)
{
	lidx = left_vertex_dist_(generator);
	//ridx = adj_list_[lidx][right_vertex_dists_[lidx](generator)];
	//lidx = left_vertex_sampler_.Sample(generator);
	int tmp = right_vertex_samplers_[lidx].Sample(generator);
	ridx = adj_list_[lidx][tmp];
	++cnts_[lidx][tmp];
	//ridx = adj_list_[lidx][rand() % num_adj_vertices_[lidx]];
}

void PairSampler::SamplePair(int &lidx, int &ridx, std::default_random_engine &generator, RandGen &rand_gen)
{
	lidx = left_vertex_dist_(generator);
	//ridx = adj_list_[lidx][right_vertex_dists_[lidx](generator)];
	//lidx = left_vertex_sampler_.Sample(generator);
	int tmp = right_vertex_samplers_[lidx].Sample(rand_gen);
	ridx = adj_list_[lidx][tmp];
	++cnts_[lidx][tmp];
	//ridx = adj_list_[lidx][rand() % num_adj_vertices_[lidx]];
}

int PairSampler::SampleRight(int lidx, RandGen &rand_gen)
{
	if (num_adj_vertices_[lidx] == 0)
		return -1;

	int tmp = right_vertex_samplers_[lidx].Sample(rand_gen);
	int ridx = adj_list_[lidx][tmp];
	++cnts_[lidx][tmp];

	return ridx;
}
