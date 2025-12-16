#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Hash mirrors the Go type [32]byte.
typedef struct {
	uint8_t bytes[32];
} Hash;

// Proof mirrors the Go Proof struct with explicit lengths.
typedef struct {
	const uint64_t *targets;
	size_t targets_len;
	const Hash *proof_hashes;
	size_t proof_len;
} Proof;

typedef struct {
	uint64_t *positions;
	Hash *hashes;
	size_t len;
	size_t cap;
} HashAndPos;

typedef Hash (*ParentHashFn)(const Hash *left, const Hash *right);

enum {
	CALC_OK = 0,
	CALC_ERR_INVALID_ARGUMENT = 1,
	CALC_ERR_PROOF_TOO_SHORT = 2,
	CALC_ERR_OOM = 3,
};

typedef struct {
	HashAndPos intermediates;
	Hash *root_hashes;
	size_t root_hashes_len;
	int error;
} CalculateHashesResult;

static const Hash HASH_EMPTY = {{0}};

static uint64_t shift_left(uint64_t value, unsigned int shift) {
	if (shift >= 64) {
		return 0;
	}
	return value << shift;
}

static uint64_t forest_mask(uint8_t forest_rows) {
	return shift_left(UINT64_C(1), (unsigned int)(forest_rows + 1)) - 1;
}

static bool hash_is_empty(const Hash *hash) {
	return memcmp(hash->bytes, HASH_EMPTY.bytes, sizeof(hash->bytes)) == 0;
}

static int hash_and_pos_reserve(HashAndPos *hp, size_t new_cap) {
	if (new_cap <= hp->cap) {
		return 0;
	}

	uint64_t *positions = malloc(new_cap * sizeof(uint64_t));
	if (new_cap && !positions) {
		return -1;
	}
	Hash *hashes = malloc(new_cap * sizeof(Hash));
	if (new_cap && !hashes) {
		free(positions);
		return -1;
	}

	if (hp->len > 0) {
		memcpy(positions, hp->positions, hp->len * sizeof(uint64_t));
		memcpy(hashes, hp->hashes, hp->len * sizeof(Hash));
	}

	free(hp->positions);
	free(hp->hashes);
	hp->positions = positions;
	hp->hashes = hashes;
	hp->cap = new_cap;
	return 0;
}

static int hash_and_pos_init(HashAndPos *hp, size_t capacity) {
	hp->positions = NULL;
	hp->hashes = NULL;
	hp->len = 0;
	hp->cap = 0;
	if (capacity == 0) {
		return 0;
	}
	return hash_and_pos_reserve(hp, capacity);
}

static void hash_and_pos_free(HashAndPos *hp) {
	free(hp->positions);
	free(hp->hashes);
	hp->positions = NULL;
	hp->hashes = NULL;
	hp->len = 0;
	hp->cap = 0;
}

static int hash_and_pos_append(HashAndPos *hp, uint64_t pos, const Hash *hash) {
	if (hp->len >= hp->cap) {
		size_t new_cap = hp->cap == 0 ? 4 : hp->cap * 2;
		if (new_cap < hp->len + 1) {
			new_cap = hp->len + 1;
		}
		if (hash_and_pos_reserve(hp, new_cap) != 0) {
			return -1;
		}
	}

	hp->positions[hp->len] = pos;
	hp->hashes[hp->len] = *hash;
	hp->len++;
	return 0;
}

typedef struct {
	uint64_t pos;
	Hash hash;
} HashPosPair;

static int compare_hash_pos_pairs(const void *a, const void *b) {
	const HashPosPair *pa = (const HashPosPair *)a;
	const HashPosPair *pb = (const HashPosPair *)b;
	if (pa->pos < pb->pos) {
		return -1;
	}
	if (pa->pos > pb->pos) {
		return 1;
	}
	return 0;
}

static int hash_and_pos_from_targets(const uint64_t *targets, size_t targets_len,
				     const Hash *hashes, size_t hashes_len,
				     HashAndPos *out) {
	if (targets_len == 0) {
		return hash_and_pos_init(out, 0);
	}
	if (!targets || !hashes || hashes_len != targets_len) {
		return -1;
	}

	HashPosPair *pairs = malloc(targets_len * sizeof(HashPosPair));
	if (!pairs) {
		return -1;
	}

	for (size_t i = 0; i < targets_len; i++) {
		pairs[i].pos = targets[i];
		pairs[i].hash = hashes[i];
	}
	qsort(pairs, targets_len, sizeof(HashPosPair), compare_hash_pos_pairs);

	if (hash_and_pos_init(out, targets_len) != 0) {
		free(pairs);
		return -1;
	}

	out->len = targets_len;
	for (size_t i = 0; i < targets_len; i++) {
		out->positions[i] = pairs[i].pos;
		out->hashes[i] = pairs[i].hash;
	}

	free(pairs);
	return 0;
}

static int hash_and_pos_merge_sorted(const HashAndPos *a, const HashAndPos *b,
				     HashAndPos *out) {
	const size_t maxa = a->len;
	const size_t maxb = b->len;

	if (hash_and_pos_init(out, maxa + maxb) != 0) {
		return -1;
	}

	size_t idxa = 0, idxb = 0, j = 0;
	while (idxa < maxa || idxb < maxb) {
		if (idxa >= maxa) {
			const size_t remain = maxb - idxb;
			memcpy(out->positions + j, b->positions + idxb, remain * sizeof(uint64_t));
			memcpy(out->hashes + j, b->hashes + idxb, remain * sizeof(Hash));
			j += remain;
			break;
		}
		if (idxb >= maxb) {
			const size_t remain = maxa - idxa;
			memcpy(out->positions + j, a->positions + idxa, remain * sizeof(uint64_t));
			memcpy(out->hashes + j, a->hashes + idxa, remain * sizeof(Hash));
			j += remain;
			break;
		}

		const uint64_t vala = a->positions[idxa];
		const uint64_t valb = b->positions[idxb];
		if (vala < valb) {
			out->positions[j] = vala;
			out->hashes[j] = a->hashes[idxa];
			idxa++;
			j++;
		} else if (vala > valb) {
			out->positions[j] = valb;
			out->hashes[j] = b->hashes[idxb];
			idxb++;
			j++;
		} else {
			out->positions[j] = vala;
			out->hashes[j] = a->hashes[idxa];
			idxa++;
			idxb++;
			j++;
		}
	}

	out->len = j;
	return 0;
}

static int next_least_slice(const uint64_t *slice1, size_t len1,
			    const uint64_t *slice2, size_t len2,
			    size_t idx1, size_t idx2) {
	const bool slice1_valid = idx1 < len1;
	const bool slice2_valid = idx2 < len2;
	if (slice1_valid && slice2_valid) {
		return slice1[idx1] < slice2[idx2] ? 0 : 1;
	}
	if (slice1_valid) {
		return 0;
	}
	if (slice2_valid) {
		return 1;
	}
	return -1;
}

static uint64_t right_sib(uint64_t pos) {
	return pos | 1ULL;
}

static int get_next_pos(const uint64_t *slice1, size_t len1,
			const uint64_t *slice2, size_t len2,
			size_t idx1, size_t idx2,
			uint64_t *pos_out, int *idx_out, int *sib_idx_out) {
	int idx = next_least_slice(slice1, len1, slice2, len2, idx1, idx2);
	if (idx == 0) {
		*pos_out = slice1[idx1];
		idx1++;
	} else if (idx == 1) {
		*pos_out = slice2[idx2];
		idx2++;
	} else {
		*idx_out = -1;
		*sib_idx_out = -1;
		return 0;
	}

	int sib_idx = next_least_slice(slice1, len1, slice2, len2, idx1, idx2);
	if (sib_idx == 0) {
		if (idx1 >= len1 || right_sib(*pos_out) != slice1[idx1]) {
			sib_idx = -1;
		}
	} else if (sib_idx == 1) {
		if (idx2 >= len2 || right_sib(*pos_out) != slice2[idx2]) {
			sib_idx = -1;
		}
	}

	*idx_out = idx;
	*sib_idx_out = sib_idx;
	return 0;
}

static bool is_left_niece(uint64_t position) {
	return (position & 1ULL) == 0;
}

static Hash get_next_hash(uint64_t pos, const Hash *hash, const Hash *sib_hash,
			  ParentHashFn parent_hash_fn) {
	if (hash_is_empty(hash)) {
		return *sib_hash;
	}
	if (hash_is_empty(sib_hash)) {
		return *hash;
	}

	if (is_left_niece(pos)) {
		return parent_hash_fn(hash, sib_hash);
	}
	return parent_hash_fn(sib_hash, hash);
}

static uint8_t tree_rows(uint64_t n) {
	if (n == 0) {
		return 0;
	}
	const uint64_t value = n - 1;
	if (value == 0) {
		return 0;
	}
	return (uint8_t)(64 - __builtin_clzll(value));
}

static uint8_t num_roots(uint64_t num_leaves) {
	return (uint8_t)__builtin_popcountll(num_leaves);
}

static uint64_t parent(uint64_t position, uint8_t forest_rows) {
	return (position >> 1) | shift_left(UINT64_C(1), forest_rows);
}

static uint64_t parent_many(uint64_t position, uint8_t rise, uint8_t forest_rows) {
	if (rise == 0) {
		return position;
	}

	const uint64_t mask = forest_mask(forest_rows);
	const uint64_t shifted = position >> rise;
	int shift = (int)forest_rows - ((int)rise - 1);
	if (shift < 0) {
		shift = 0;
	}
	const uint64_t filled = shift_left(mask, (unsigned int)shift);
	return (shifted | filled) & mask;
}

static uint64_t max_position_at_row(uint8_t row, uint8_t forest_rows, uint64_t num_leaves) {
	uint64_t max = parent_many(num_leaves, row, forest_rows);
	if (max != 0) {
		max -= 1;
	}
	return max;
}

static uint64_t root_position(uint64_t leaves, uint8_t row, uint8_t forest_rows) {
	const uint64_t mask = forest_mask(forest_rows);
	const uint64_t before = leaves & shift_left(mask, (unsigned int)(row + 1));
	const uint64_t shifted = (before >> row) | shift_left(mask, (unsigned int)(forest_rows + 1 - row));
	return shifted & mask;
}

static bool is_root_position_on_row(uint64_t position, uint64_t num_leaves, uint8_t row) {
	const bool root_present = (num_leaves >> row) & 1ULL;
	if (!root_present) {
		return false;
	}
	const uint8_t rows = tree_rows(num_leaves);
	return root_position(num_leaves, row, rows) == position;
}

CalculateHashesResult calculate_hashes(uint64_t num_leaves, const Hash *del_hashes,
				       size_t del_hashes_len, const Proof *proof,
				       ParentHashFn parent_hash_fn) {
	CalculateHashesResult result = {0};
	if (!proof || (!proof->targets && proof->targets_len > 0) ||
	    (!proof->proof_hashes && proof->proof_len > 0) || !parent_hash_fn) {
		result.error = CALC_ERR_INVALID_ARGUMENT;
		return result;
	}

	const size_t target_len = proof->targets_len;
	const uint8_t total_rows = tree_rows(num_leaves);

	HashAndPos next_proves;
	if (hash_and_pos_init(&next_proves, target_len) != 0) {
		result.error = CALC_ERR_OOM;
		return result;
	}

	HashAndPos to_prove;
	const Hash *leaf_hashes = del_hashes;
	Hash *owned_hashes = NULL;
	if (target_len > 0 && !del_hashes) {
		owned_hashes = calloc(target_len, sizeof(Hash));
		if (!owned_hashes) {
			hash_and_pos_free(&next_proves);
			result.error = CALC_ERR_OOM;
			return result;
		}
		leaf_hashes = owned_hashes;
	} else if (target_len != del_hashes_len) {
		hash_and_pos_free(&next_proves);
		result.error = CALC_ERR_INVALID_ARGUMENT;
		return result;
	}

	if (hash_and_pos_from_targets(proof->targets, target_len, leaf_hashes,
				      target_len, &to_prove) != 0) {
		free(owned_hashes);
		hash_and_pos_free(&next_proves);
		result.error = CALC_ERR_OOM;
		return result;
	}

	const size_t roots_cap = num_roots(num_leaves);
	Hash *calculated_roots = NULL;
	if (roots_cap > 0) {
		calculated_roots = malloc(roots_cap * sizeof(Hash));
		if (!calculated_roots) {
			hash_and_pos_free(&to_prove);
			hash_and_pos_free(&next_proves);
			free(owned_hashes);
			result.error = CALC_ERR_OOM;
			return result;
		}
	}
	size_t calculated_root_len = 0;

	size_t proof_hash_idx = 0;
	size_t to_prove_idx = 0;
	size_t next_proves_idx = 0;
	uint8_t row = 0;

	while (row <= total_rows) {
		uint64_t prove_pos = 0;
		int idx = -1;
		int sib_idx = -1;
		get_next_pos(to_prove.positions, to_prove.len, next_proves.positions, next_proves.len,
			     to_prove_idx, next_proves_idx, &prove_pos, &idx, &sib_idx);
		if (idx == -1) {
			break;
		}

		Hash prove_hash;
		if (idx == 0) {
			prove_hash = to_prove.hashes[to_prove_idx];
			to_prove_idx++;
		} else {
			prove_hash = next_proves.hashes[next_proves_idx];
			next_proves_idx++;
		}

		uint64_t max_pos = max_position_at_row(row, total_rows, num_leaves);
		while (prove_pos > max_pos && row < total_rows) {
			row++;
			max_pos = max_position_at_row(row, total_rows, num_leaves);
		}

		if (is_root_position_on_row(prove_pos, num_leaves, row)) {
			if (calculated_root_len < roots_cap) {
				calculated_roots[calculated_root_len++] = prove_hash;
			}
			continue;
		}

		Hash sib_hash = HASH_EMPTY;
		const bool sib_present = sib_idx != -1;
		if (sib_present) {
			if (sib_idx == 0) {
				sib_hash = to_prove.hashes[to_prove_idx];
				to_prove_idx++;
			} else {
				sib_hash = next_proves.hashes[next_proves_idx];
				next_proves_idx++;
			}
		} else {
			if (proof_hash_idx >= proof->proof_len) {
				free(calculated_roots);
				hash_and_pos_free(&to_prove);
				hash_and_pos_free(&next_proves);
				free(owned_hashes);
				result.error = CALC_ERR_PROOF_TOO_SHORT;
				return result;
			}
			sib_hash = proof->proof_hashes[proof_hash_idx++];
		}

		const Hash next_hash = get_next_hash(prove_pos, &prove_hash, &sib_hash, parent_hash_fn);
		if (hash_and_pos_append(&next_proves, parent(prove_pos, total_rows), &next_hash) != 0) {
			free(calculated_roots);
			hash_and_pos_free(&to_prove);
			hash_and_pos_free(&next_proves);
			free(owned_hashes);
			result.error = CALC_ERR_OOM;
			return result;
		}
	}

	HashAndPos merged;
	if (hash_and_pos_merge_sorted(&next_proves, &to_prove, &merged) != 0) {
		free(calculated_roots);
		hash_and_pos_free(&to_prove);
		hash_and_pos_free(&next_proves);
		free(owned_hashes);
		result.error = CALC_ERR_OOM;
		return result;
	}

	hash_and_pos_free(&to_prove);
	hash_and_pos_free(&next_proves);
	free(owned_hashes);

	result.intermediates = merged;
	result.root_hashes = calculated_roots;
	result.root_hashes_len = calculated_root_len;
	result.error = CALC_OK;
	return result;
}
