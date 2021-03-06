#include "eadocvectrainer.h"

#include <cassert>
#include <thread>

#include "negtrain.h"
#include "ioutils.h"

EADocVecTrainer::EADocVecTrainer(int num_rounds, int num_threads, int num_negative_samples, 
	float starting_alpha, float min_alpha) : num_rounds_(num_rounds), num_threads_(num_threads),
	num_negative_samples_(num_negative_samples), starting_alpha_(starting_alpha), min_alpha_(min_alpha)
{
}

void EADocVecTrainer::AllJointThreaded(const char *ee_file, const char *de_file,
	const char *dw_file, const char *entity_cnts_file, const char *word_cnts_file, 
	int vec_dim, bool shared, float weight_ee, float weight_de, float weight_dw, const char *dst_dedw_vec_file_name, 
	const char *dst_word_vecs_file_name, const char *dst_entity_vecs_file_name)
{
	initDocEntityList(de_file);
	initDocWordList(dw_file);
	initEntityEntityList(ee_file);

	entity_vec_dim_ = word_vec_dim_ = vec_dim;

	printf("initing model....\n");
	word_vecs_ = NegTrain::GetInitedVecs0(num_words_, word_vec_dim_);
	dw_vecs_ = NegTrain::GetInitedVecs0(num_docs_, word_vec_dim_);

	ee_vecs0_ = NegTrain::GetInitedVecs0(num_entities_, entity_vec_dim_);
	ee_vecs1_ = NegTrain::GetInitedVecs1(num_entities_, entity_vec_dim_);

	if (shared)
		de_vecs_ = dw_vecs_;
	else
		de_vecs_ = NegTrain::GetInitedVecs0(num_docs_, entity_vec_dim_);

	ExpTable exp_table;
	NegTrain entity_ns_trainer(&exp_table, num_negative_samples_,
		entity_cnts_file);
	NegTrain word_ns_trainer(&exp_table, num_negative_samples_,
		word_cnts_file);
	printf("inited.\n");

	int sum_ee_weights = ee_sampler_->sum_weights();
	//sum_ee_weights = 0;
	int sum_de_weights = de_sampler_->sum_weights();
	//int sum_de_weights = 0;
	int sum_dw_weights = dw_sampler_->sum_weights();
	//sum_dw_weights /= 10;
	//sum_dw_weights = 0;
	long long sum_weights = sum_ee_weights + sum_de_weights + sum_dw_weights;
	long long num_samples_per_round = sum_weights / 2;
	//long long num_samples_per_round = sum_dw_weights;
	//int num_samples_per_round = sum_ee_weights + sum_de_weights;

	printf("list_samples: %d %d %d\n", sum_ee_weights, sum_de_weights, sum_dw_weights);
	printf("%lld samples per round\n", num_samples_per_round);

	float weight_portions[] = { (float)sum_ee_weights / sum_weights,
		(float)sum_de_weights / sum_weights, (float)sum_dw_weights / sum_weights };
	printf("list distribution: %f %f %f\n", weight_portions[0], weight_portions[1],
		weight_portions[2]);
	std::discrete_distribution<int> list_sample_dist(weight_portions, weight_portions + 3);
	//std::discrete_distribution<int> list_sample_dist{ 0, 0, 1 };

	int seeds[] = { 317, 7, 31, 297, 1238, 23487, 238593, 92384, 129380, 23848 };
	std::thread *threads = new std::thread[num_threads_];
	for (int i = 0; i < num_threads_; ++i)
	{
		int cur_seed = seeds[i];
		threads[i] = std::thread([&, cur_seed, num_samples_per_round, weight_ee, weight_de, weight_dw]
		{
			allJoint(cur_seed, num_samples_per_round, weight_ee, weight_de, weight_dw,
				list_sample_dist, entity_ns_trainer, word_ns_trainer);
		});
	}
	for (int i = 0; i < num_threads_; ++i)
		threads[i].join();
	printf("\n");

	//printf("dw0: %d\n", dw_sampler_->CountZeros());
	//printf("de0: %d\n", de_sampler_->CountZeros());
	//printf("ee0: %d\n", ee_sampler_->CountZeros());

	if (shared)
		IOUtils::SaveVectors(dw_vecs_, word_vec_dim_, num_docs_, dst_dedw_vec_file_name);
	else
		saveConcatnatedVectors(de_vecs_, dw_vecs_, num_docs_, entity_vec_dim_, dst_dedw_vec_file_name);

	IOUtils::SaveVectors(word_vecs_, word_vec_dim_, num_words_, dst_word_vecs_file_name);
	IOUtils::SaveVectors(ee_vecs0_, entity_vec_dim_, num_entities_, dst_entity_vecs_file_name);
}

void EADocVecTrainer::TrainWEFixed(const char *doc_words_file, const char *doc_entities_file, const char *word_cnts_file,
	const char *entity_cnts_file, const char *word_vecs_file_name, const char *entity_vecs_file_name,
	int vec_dim, const char *dst_doc_vecs_file)
{
	initDocWordList(doc_words_file);
	initDocEntityList(doc_entities_file);

	word_vec_dim_ = vec_dim;

	printf("initing model....\n");
	int tmp_num = 0, tmp_dim = 0;
	IOUtils::LoadVectors(word_vecs_file_name, tmp_num, tmp_dim, word_vecs_);
	if (tmp_num != num_words_ || tmp_dim != vec_dim)
	{
		printf("num words: %d %d\n", num_words_, tmp_num);
		printf("vec dim: %d %d\n", vec_dim, tmp_dim);
		return;
	}

	IOUtils::LoadVectors(entity_vecs_file_name, tmp_num, tmp_dim, ee_vecs0_);
	if (tmp_num != num_entities_ || tmp_dim != vec_dim)
	{
		printf("num entities: %d %d\n", num_words_, tmp_num);
		printf("vec dim: %d %d\n", vec_dim, tmp_dim);
		return;
	}

	//dw_vecs_ = NegTrain::GetInitedVecs0(num_docs_, word_vec_dim_);
	dw_vecs_ = NegTrain::GetInitedVecs1(num_docs_, word_vec_dim_);
	de_vecs_ = dw_vecs_;
	printf("inited.\n");

	trainDWEMT(word_cnts_file, entity_cnts_file, false, false, dst_doc_vecs_file);
}

void EADocVecTrainer::TrainDocWord(const char *doc_words_file_name, const char *word_cnts_file, int vec_dim,
	const char *dst_doc_vecs_file_name, const char *dst_word_vecs_file_name)
{
	initDocWordList(doc_words_file_name);

	word_vec_dim_ = vec_dim;

	printf("initing model....\n");
	word_vecs_ = NegTrain::GetInitedVecs0(num_words_, word_vec_dim_);
	dw_vecs_ = NegTrain::GetInitedVecs0(num_docs_, word_vec_dim_);
	printf("inited.\n");

	trainDocWordMT(word_cnts_file, true, dst_doc_vecs_file_name);

	if (dst_word_vecs_file_name != 0)
		IOUtils::SaveVectors(word_vecs_, word_vec_dim_, num_words_, dst_word_vecs_file_name);
}

void EADocVecTrainer::TrainEmadrNewDocs2(const char * doc_words_file, const char * doc_entities_file, const char * word_cnts_file, 
	const char * entity_cnts_file, const char * word_vecs_file_name, const char * entity_vecs_file_name, int vec_dim, 
	const char * dst_doc_vecs_file)
{
	TrainDocWordFixedWordVecs(doc_entities_file, entity_cnts_file, entity_vecs_file_name,
		vec_dim, 0);

	//for (int i = 0; i < 5; ++i) {
	//	for (int j = 0; j < vec_dim; ++j) {
	//		printf("%f ", dw_vecs_[i][j]);
	//	}
	//	printf("\n");
	//}

	float **tmpvecs = dw_vecs_;
	dw_vecs_ = de_vecs_;
	de_vecs_ = tmpvecs;
	TrainDocWordFixedWordVecs(doc_words_file, word_cnts_file, word_vecs_file_name,
		vec_dim, 0);
	saveConcatnatedVectors(de_vecs_, dw_vecs_, num_docs_, vec_dim, dst_doc_vecs_file);
}

void EADocVecTrainer::TrainDocWordFixedWordVecs(const char *doc_words_file_name, const char *word_cnts_file,
	const char *word_vecs_file_name, int vec_dim, const char *dst_doc_vecs_file_name)
{
	initDocWordList(doc_words_file_name);

	word_vec_dim_ = vec_dim;

	printf("initing model....\n");
	int tmp_num = 0, tmp_dim = 0;
	IOUtils::LoadVectors(word_vecs_file_name, tmp_num, tmp_dim, word_vecs_);
	if (tmp_num != num_words_ || tmp_dim != vec_dim)
	{
		printf("num words: %d %d\n", num_words_, tmp_num);
		printf("vec dim: %d %d\n", vec_dim, tmp_dim);
		return;
	}
	//dw_vecs_ = NegTrain::GetInitedVecs0(num_docs_, word_vec_dim_);
	dw_vecs_ = NegTrain::GetInitedVecs1(num_docs_, word_vec_dim_);
	printf("inited.\n");

	trainDocWordMT(word_cnts_file, false, dst_doc_vecs_file_name);

	//for (int i = 0; i < 5; ++i) {
	//	for (int j = 0; j < vec_dim; ++j) {
	//		printf("%f ", dw_vecs_[i][j]);
	//	}
	//	printf("\n");
	//}
}

void EADocVecTrainer::saveConcatnatedVectors(float **vecs0, float **vecs1, int num_vecs, int vec_dim,
	const char *dst_file_name)
{
	FILE *fp = fopen(dst_file_name, "wb");
	assert(fp != 0);

	fwrite(&num_vecs, 4, 1, fp);
	int full_vec_dim = vec_dim << 1;
	fwrite(&full_vec_dim, 4, 1, fp);

	for (int i = 0; i < num_vecs; ++i)
	{
		fwrite(vecs0[i], 4, vec_dim, fp);
		fwrite(vecs1[i], 4, vec_dim, fp);
	}

	fclose(fp);
}

void EADocVecTrainer::allJoint(int seed, long long num_samples_per_round, float weight_ee, float weight_de, float weight_dw,
	std::discrete_distribution<int> &list_sample_dist,
	NegTrain &entity_ns_trainer, NegTrain &word_ns_trainer)
{
	//printf("seed %d samples_per_round %d. training...\n", seed, num_samples_per_round);
	std::default_random_engine generator(seed);

	RandGen rand_gen(seed);

	//const float min_alpha = starting_alpha_ * 0.001;
	long long total_num_samples = num_rounds_ * num_samples_per_round;

	float *tmp_neu1e = new float[entity_vec_dim_];

	float alpha = starting_alpha_;
	for (int i = 0; i < num_rounds_; ++i)
	{
		printf("\rround %d, alpha %f", i, alpha);
		fflush(stdout);
		for (int j = 0; j < num_samples_per_round; ++j)
		{
			long long cur_num_samples = (i * num_samples_per_round) + j;
			if (cur_num_samples % 10000 == 10000 - 1)
				alpha = starting_alpha_ + (min_alpha_ - starting_alpha_) * cur_num_samples / total_num_samples;

			int list_idx = list_sample_dist(generator);
			int va = 0, vb = 0;
			if (list_idx == 0)
			{
				ee_sampler_->SamplePair(va, vb, generator, rand_gen);
				entity_ns_trainer.TrainPair(entity_vec_dim_, ee_vecs0_[va], vb, ee_vecs1_,
					alpha, tmp_neu1e, generator, weight_ee);
				entity_ns_trainer.TrainPair(entity_vec_dim_, ee_vecs0_[vb], va, ee_vecs1_,
					alpha, tmp_neu1e, generator, weight_ee);
			}
			else if (list_idx == 1)
			{
				de_sampler_->SamplePair(va, vb, generator, rand_gen);
				entity_ns_trainer.TrainPair(entity_vec_dim_, de_vecs_[va], vb, ee_vecs0_,
					alpha, tmp_neu1e, generator, weight_de);
			}
			else if (list_idx == 2)
			{
				dw_sampler_->SamplePair(va, vb, generator, rand_gen);
				word_ns_trainer.TrainPair(word_vec_dim_, dw_vecs_[va], vb, word_vecs_,
					alpha, tmp_neu1e, generator, weight_dw);
			}
		}
	}

	delete[] tmp_neu1e;
}

void EADocVecTrainer::trainDocWordMT(const char *word_cnts_file, bool update_word_vecs, const char *dst_doc_vecs_file_name)
{
	ExpTable exp_table;
	NegTrain word_ns_trainer(&exp_table, num_negative_samples_, word_cnts_file);

	int sum_dw_weights = dw_sampler_->sum_weights();
	long long num_samples_per_round = sum_dw_weights / 2;

	printf("%lld samples per round\n", num_samples_per_round);

	int seeds[] = { 317, 7, 31, 297, 1238, 23487, 238593, 92384, 129380, 23848 };
	std::thread *threads = new std::thread[num_threads_];
	for (int i = 0; i < num_threads_; ++i)
	{
		int cur_seed = seeds[i];
		threads[i] = std::thread([&, cur_seed, num_samples_per_round, update_word_vecs]
		{
			trainDocWordList(cur_seed, num_samples_per_round, update_word_vecs, word_ns_trainer);
		});
	}
	for (int i = 0; i < num_threads_; ++i)
		threads[i].join();
	printf("\n");

	if (dst_doc_vecs_file_name)
		IOUtils::SaveVectors(dw_vecs_, word_vec_dim_, num_docs_, dst_doc_vecs_file_name);
}

void EADocVecTrainer::trainDocWordList(int seed, long long num_samples_per_round, bool update_word_vecs, 
	NegTrain &word_ns_trainer)
{
	//printf("seed %d samples_per_round %d. training...\n", seed, num_samples_per_round);
	std::default_random_engine generator(seed);

	RandGen rand_gen(seed);

	//const float min_alpha = starting_alpha_ * 0.001;
	long long total_num_samples = num_rounds_ * num_samples_per_round;

	float *tmp_neu1e = new float[word_vec_dim_];

	float alpha = starting_alpha_;
	int va = 0, vb = 0;
	for (int i = 0; i < num_rounds_; ++i)
	{
		printf("\rround %d, alpha %f", i, alpha);
		fflush(stdout);
		for (int j = 0; j < num_samples_per_round; ++j)
		{
			long long cur_num_samples = (i * num_samples_per_round) + j;
			if (cur_num_samples % 10000 == 10000 - 1)
				alpha = starting_alpha_ + (min_alpha_ - starting_alpha_) * cur_num_samples / total_num_samples;

			dw_sampler_->SamplePair(va, vb, generator, rand_gen);
			//if (va == 0)
			//	printf("%d %d\n", va, vb);
			word_ns_trainer.TrainPair(word_vec_dim_, dw_vecs_[va], vb, word_vecs_,
				alpha, tmp_neu1e, generator, 1, true, update_word_vecs);
		}
	}

	delete[] tmp_neu1e;
}

void EADocVecTrainer::trainDWEMT(const char *word_cnts_file, const char *entity_cnts_file, bool update_word_vecs, 
	bool update_entity_vecs, const char *dst_doc_vecs_file_name)
{
	ExpTable exp_table;
	NegTrain word_ns_trainer(&exp_table, num_negative_samples_, word_cnts_file);
	NegTrain entity_ns_trainer(&exp_table, num_negative_samples_, entity_cnts_file);

	int sum_dw_weights = dw_sampler_->sum_weights();
	int sum_de_weights = de_sampler_->sum_weights();
	long long sum_weights = sum_dw_weights + sum_de_weights;
	long long num_samples_per_round = sum_weights / 2;

	float weight_portions[] = { (float)sum_de_weights / sum_weights, (float)sum_dw_weights / sum_weights };
	printf("list distribution: %f %f\n", weight_portions[0], weight_portions[1]);
	std::discrete_distribution<int> list_sample_dist(weight_portions, weight_portions + 2);

	printf("%lld samples per round\n", num_samples_per_round);

	int seeds[] = { 317, 7, 31, 297, 1238, 23487, 238593, 92384, 129380, 23848 };
	std::thread *threads = new std::thread[num_threads_];
	for (int i = 0; i < num_threads_; ++i)
	{
		int cur_seed = seeds[i];
		threads[i] = std::thread([&, cur_seed, num_samples_per_round, update_word_vecs, update_entity_vecs]
		{
			trainDWETh(cur_seed, num_samples_per_round, update_word_vecs, update_entity_vecs, list_sample_dist,
				word_ns_trainer, entity_ns_trainer);
		});
	}
	for (int i = 0; i < num_threads_; ++i)
		threads[i].join();
	printf("\n");

	IOUtils::SaveVectors(dw_vecs_, word_vec_dim_, num_docs_, dst_doc_vecs_file_name);
}

void EADocVecTrainer::trainDWETh(int seed, long long num_samples_per_round, bool update_word_vecs, bool update_entity_vecs, std::discrete_distribution<int> &list_sample_dist,
	NegTrain &word_ns_trainer, NegTrain &entity_ns_trainer)
{
	//printf("seed %d samples_per_round %d. training...\n", seed, num_samples_per_round);
	std::default_random_engine generator(seed);

	RandGen rand_gen(seed);

	//const float min_alpha = starting_alpha_ * 0.001;
	long long total_num_samples = num_rounds_ * num_samples_per_round;

	float *tmp_neu1e = new float[word_vec_dim_];

	float alpha = starting_alpha_;
	int va = 0, vb = 0;
	for (int i = 0; i < num_rounds_; ++i)
	{
		printf("\rround %d, alpha %f", i, alpha);
		fflush(stdout);
		for (int j = 0; j < num_samples_per_round; ++j)
		{
			long long cur_num_samples = (i * num_samples_per_round) + j;
			if (cur_num_samples % 10000 == 10000 - 1)
				alpha = starting_alpha_ + (min_alpha_ - starting_alpha_) * cur_num_samples / total_num_samples;

			int list_idx = list_sample_dist(generator);
			int va = 0, vb = 0;
			if (list_idx == 0)
			{
				de_sampler_->SamplePair(va, vb, generator, rand_gen);
				entity_ns_trainer.TrainPair(entity_vec_dim_, de_vecs_[va], vb, ee_vecs0_,
					alpha, tmp_neu1e, generator, 1, true, update_entity_vecs);
			}
			else if (list_idx == 1)
			{
				dw_sampler_->SamplePair(va, vb, generator, rand_gen);
				word_ns_trainer.TrainPair(word_vec_dim_, dw_vecs_[va], vb, word_vecs_,
					alpha, tmp_neu1e, generator, 1, true, update_word_vecs);
			}
		}
	}

	delete[] tmp_neu1e;
}
