#ifndef JOINT_TRAINER_H_
#define JOINT_TRAINER_H_

#include <random>
#include "negative_sampling_trainer.h"
#include "edge_net.h"
#include "adj_list_net.h"

class JointTrainer
{
public:
	JointTrainer(const char *ee_net_file_name, const char *doc_entity_net_file_name,
		const char *doc_words_file_name);
	~JointTrainer();

	void JointTrainingThreaded(int vec_dim, int num_rounds, int num_threads, int num_negative_samples,
		const char *dst_doc_vec_file_name);
	void JointTraining(int seed, int num_rounds, int num_samples_per_round, std::discrete_distribution<int> &ee_edge_sample_dist,
		std::discrete_distribution<int> &de_edge_sample_dist, std::discrete_distribution<int> &dw_edge_sample_dist, std::discrete_distribution<int> &net_sample_dist,
		NegativeSamplingTrainer &entity_ns_trainer, NegativeSamplingTrainer &word_ns_trainer);

	void TrainEntityNetThreaded(int vec_dim, int num_rounds, int num_threads, int num_negative_samples,
		const char *dst_input_vec_file_name, const char *dst_output_vecs_file_name);

	void TrainEntityNet(int seed, int num_training_rounds, int num_samples_per_round, std::discrete_distribution<int> &ee_edge_sample_dist,
		NegativeSamplingTrainer &ns_trainer);

	void TrainDocEntityNetThreaded(const char *entity_vec_file_name, int num_rounds,
		int num_negative_samples, int num_threads, const char *dst_doc_vec_file_name);

	void TrainDocEntityNet(int seed, int num_rounds, int num_samples_per_round, std::discrete_distribution<int> &edge_sample_dist,
		NegativeSamplingTrainer &ns_trainer);

private:

	//void saveVectors(float **vecs, int vec_len, int num_vecs, const char *dst_file_name);

private:
	// Entity net
	EdgeNet entity_net_;
	int num_entities_ = 0;
	int ee_vec_dim_ = 0;

	//std::discrete_distribution<int> ee_edge_sample_dist_;
	std::discrete_distribution<int> entity_sample_dist_;

	EdgeNet entity_doc_net_;
	int num_docs_ = 0;

	EdgeNet doc_word_net_;
	int num_words_ = 0;

	float **ee_vecs0_ = 0;
	float **ee_vecs1_ = 0;

	float **ed_vecs_ = 0;

	float **word_vecs_ = 0;
	std::discrete_distribution<int> word_sample_dist_;
};

#endif