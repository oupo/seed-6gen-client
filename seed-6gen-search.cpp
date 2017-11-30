#include <cstdio>
#include <cstdint>
#include <array>
#include <string>
#include <random>
#include <chrono>
#include <omp.h>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

constexpr size_t NN = 4500;
constexpr size_t MM = 700;

constexpr uint32_t BLOCK_SIZE = 0x10000;

string tohex(uint32_t x) {
	ostringstream ostr;
	ostr << hex << setfill('0') << setw(8) << x;
	return ostr.str();
}

int upper(uint32_t x, int n) {
	return (uint64_t)x * n >> 32;
}

struct Target {
	int id;
	uint32_t iv0;
	uint32_t iv1;
};

uint32_t encode_iv(int *ivs) {
	return ivs[0] | (ivs[1] << 5) | (ivs[2] << 10) | (ivs[3] << 15) | (ivs[4] << 20) | (ivs[5] << 25);
}

const int N = 624, M = 397;
const uint32_t UPPER_MASK = 0x80000000UL;
const uint32_t LOWER_MASK = 0x7fffffffUL;
const uint32_t mag01[2] = { 0x0UL, 0x9908b0dfUL };

void mt_fill_cont(uint32_t *rands, int start, int end) {
	uint32_t y;
	for (int i = start; i < end; i++) {
		y = (rands[i - N] & UPPER_MASK) | (rands[i - N + 1] & LOWER_MASK);
		rands[i] = rands[i + M - N] ^ (y >> 1) ^ mag01[y & 0x1UL];
	}
}

uint32_t temper(uint32_t y) {
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);
	return y;
}

void mt_fill(uint32_t seed, uint32_t *rands, int n) {
	if (n < N) throw "too small n";
	rands[0] = seed;
	for (int i = 1; i<624; i++) {
		rands[i] = (1812433253UL * (rands[i - 1] ^ (rands[i - 1] >> 30)) + i);
	}
	uint32_t y;
	int kk;
	for (kk = 0; kk<N - M; kk++) {
		y = (rands[kk] & UPPER_MASK) | (rands[kk + 1] & LOWER_MASK);
		rands[kk] = rands[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
	}
	for (; kk<N - 1; kk++) {
		y = (rands[kk] & UPPER_MASK) | (rands[kk + 1] & LOWER_MASK);
		rands[kk] = rands[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
	}
	y = (rands[N - 1] & UPPER_MASK) | (rands[0] & LOWER_MASK);
	rands[N - 1] = rands[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

	for (int i = N; i < n; i++) {
		y = (rands[i - N] & UPPER_MASK) | (rands[i - N + 1] & LOWER_MASK);
		rands[i] = rands[i + M - N] ^ (y >> 1) ^ mag01[y & 0x1UL];
	}
}

void test_seed(vector<Target> &targets, unordered_multimap<uint32_t, int> &map, uint32_t seed, ofstream &ofs, int &found) {
	array<uint32_t, NN> rand;
	array<int, NN> r;
	mt_fill(seed, rand.data(), MM);
	bool built_rand_all = false;
	for (int i = 0; i < MM; i++) {
		r[i] = upper(temper(rand[i]), 32);
	}
	for (int i = 0; i < MM - 6; i++) {
		auto it = map.find(encode_iv(r.data() + i));
		for (; it != map.end(); ++ it) {
			int k = it->second;
			if (!built_rand_all) {
				built_rand_all = true;
				mt_fill_cont(rand.data(), MM, NN);
				for (int j = MM; j < NN; j++) {
					r[j] = upper(temper(rand[j]), 32);
				}
			}
			for (int j = i + 6; j < NN - 6; j++) {
				if (encode_iv(r.data() + j) == targets[k].iv1) {
					found++;
					ofs << targets[k].id << " " << i << " " << j << " " << tohex(seed) << endl;
					ofs.flush();
				}
			}
		}
	}
}

int main() {
	int n;
	vector<Target> targets;
#ifdef _OPENMP
	fprintf(stderr, "OpenMP is enabled. max_threads=%d\n", omp_get_max_threads());
	fflush(stderr);
#endif
	

#if 0
	n = 100;
	mt19937 rnd(0);
	for (int i = 0; i < 1000; i++) {
		Target t;
		t.id = i;
		int iv[6];
		for (int j = 0; j < 6; j++) iv[j] = rnd() % 32;
		t.iv0 = encode_iv(iv);
		for (int j = 0; j < 6; j++) iv[j] = rnd() % 32;
		t.iv1 = encode_iv(iv);
		targets.push_back(t);
	}
#else
	scanf("%d", &n);
	for (int i = 0; i < n; i++) {
		Target t;
		int iv[6];
		scanf("%d", &t.id);
		for (int j = 0; j < 6; j++) scanf("%d", &iv[j]);
		t.iv0 = encode_iv(iv);
		for (int j = 0; j < 6; j++) scanf("%d", &iv[j]);
		t.iv1 = encode_iv(iv);
		targets.push_back(t);
	}
#endif
	auto start = chrono::system_clock::now();
	unordered_multimap<uint32_t, int> map;
	for (size_t i = 0; i < targets.size(); i++) {
		map.emplace(targets[i].iv0, i);
	}
	int found = 0;
	ofstream ofs("output");

	for (uint64_t count = 0; count < (1ull << 32); count += BLOCK_SIZE) {
		printf("\r%08x %d", (uint32_t)count, found);
		fflush(stdout);
#pragma omp parallel for
		for (int i = 0; i < BLOCK_SIZE; i++) {
			uint32_t seed = (uint32_t)count + i;
			test_seed(targets, map, seed, ofs, found);
		}
	}
	auto end = chrono::system_clock::now();
	auto time = chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	fprintf(stderr, "time = %lld\n", time);
	fflush(stderr);
}