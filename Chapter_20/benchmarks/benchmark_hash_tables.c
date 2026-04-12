#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

typedef struct {
  const char* name;
  int operations;
  double seconds;
} BenchmarkResult;

static double nowSeconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static ObjString* makeIndexedString(const char* prefix, int index) {
  char buffer[64];
  int length = snprintf(buffer, sizeof(buffer), "%s_%d", prefix, index);
  return copyString(buffer, length);
}

static void fillStringKeys(Value* keys, int count, const char* prefix) {
  for (int i = 0; i < count; i++) {
    keys[i] = OBJ_VAL(makeIndexedString(prefix, i));
  }
}

static void fillMixedKeys(Value* keys, int count) {
  for (int i = 0; i < count; i++) {
    switch (i % 8) {
      case 0:
        keys[i] = NUMBER_VAL(i * 1.25);
        break;
      case 1:
        keys[i] = OBJ_VAL(makeIndexedString("mix", i));
        break;
      case 2:
        keys[i] = BOOL_VAL((i / 2) % 2 == 0);
        break;
      case 3:
        keys[i] = NIL_VAL;
        break;
      case 4:
        keys[i] = NUMBER_VAL(i * 1000.0 + 0.5);
        break;
      case 5:
        keys[i] = OBJ_VAL(makeIndexedString("k", i));
        break;
      case 6:
        keys[i] = NUMBER_VAL(-i * 2.0);
        break;
      case 7:
        keys[i] = OBJ_VAL(makeIndexedString("s", i));
        break;
    }
  }
}

static int findCollisionKeys(Value* out, int wanted, int modulus) {
  int found = 0;
  int candidate = 0;
  while (found < wanted) {
    Value key = NUMBER_VAL(candidate * 0.5 + 123.0);
    if ((int)(hashValue(key) % (uint32_t)modulus) == 0) {
      out[found++] = key;
    }
    candidate++;
  }
  return found;
}

static BenchmarkResult benchmarkInsertStrings(int count) {
  Table table;
  initTable(&table);
  Value* keys = ALLOCATE(Value, count);
  fillStringKeys(keys, count, "insert");

  double start = nowSeconds();
  for (int i = 0; i < count; i++) {
    tableSet(&table, keys[i], NUMBER_VAL(i));
  }
  double end = nowSeconds();

  FREE_ARRAY(Value, keys, count);
  freeTable(&table);
  BenchmarkResult result = {"insert_strings", count, end - start};
  return result;
}

static BenchmarkResult benchmarkLookupHits(int count) {
  Table table;
  initTable(&table);
  Value* keys = ALLOCATE(Value, count);
  fillStringKeys(keys, count, "hit");
  for (int i = 0; i < count; i++) tableSet(&table, keys[i], NUMBER_VAL(i));

  volatile double sink = 0;
  Value value;
  double start = nowSeconds();
  for (int i = 0; i < count; i++) {
    if (tableGet(&table, keys[i], &value)) sink += AS_NUMBER(value);
  }
  double end = nowSeconds();
  if (sink == -1) printf("ignore %f\n", sink);

  FREE_ARRAY(Value, keys, count);
  freeTable(&table);
  BenchmarkResult result = {"lookup_hits", count, end - start};
  return result;
}

static BenchmarkResult benchmarkLookupMisses(int count) {
  Table table;
  initTable(&table);
  Value* keys = ALLOCATE(Value, count);
  Value* misses = ALLOCATE(Value, count);
  fillStringKeys(keys, count, "present");
  fillStringKeys(misses, count, "missing");
  for (int i = 0; i < count; i++) tableSet(&table, keys[i], NUMBER_VAL(i));

  volatile int found = 0;
  Value value;
  double start = nowSeconds();
  for (int i = 0; i < count; i++) {
    if (tableGet(&table, misses[i], &value)) found++;
  }
  double end = nowSeconds();
  if (found != 0) printf("unexpected hits: %d\n", found);

  FREE_ARRAY(Value, misses, count);
  FREE_ARRAY(Value, keys, count);
  freeTable(&table);
  BenchmarkResult result = {"lookup_misses", count, end - start};
  return result;
}

static BenchmarkResult benchmarkDeletionChurn(int count) {
  Table table;
  initTable(&table);
  Value* keys = ALLOCATE(Value, count);
  fillStringKeys(keys, count, "churn");
  for (int i = 0; i < count; i++) tableSet(&table, keys[i], NUMBER_VAL(i));

  double start = nowSeconds();
  for (int i = 0; i < count; i++) {
    tableDelete(&table, keys[i]);
    tableSet(&table, keys[i], NUMBER_VAL(i + 1));
  }
  double end = nowSeconds();

  FREE_ARRAY(Value, keys, count);
  freeTable(&table);
  BenchmarkResult result = {"deletion_churn", count * 2, end - start};
  return result;
}

static BenchmarkResult benchmarkMixedKeys(int count) {
  Table table;
  initTable(&table);
  Value* keys = ALLOCATE(Value, count);
  fillMixedKeys(keys, count);

  double start = nowSeconds();
  for (int i = 0; i < count; i++) {
    tableSet(&table, keys[i], NUMBER_VAL(i));
  }
  Value value;
  volatile int hits = 0;
  for (int i = 0; i < count; i++) {
    if (tableGet(&table, keys[i], &value)) hits++;
  }
  double end = nowSeconds();
  if (hits < count / 2) printf("unexpectedly low hits: %d\n", hits);

  FREE_ARRAY(Value, keys, count);
  freeTable(&table);
  BenchmarkResult result = {"mixed_keys_insert_and_hit", count * 2, end - start};
  return result;
}

static BenchmarkResult benchmarkCollisionHeavy(int count) {
  Table table;
  initTable(&table);
  Value* keys = ALLOCATE(Value, count);
  findCollisionKeys(keys, count, 256);

  double start = nowSeconds();
  for (int i = 0; i < count; i++) {
    tableSet(&table, keys[i], NUMBER_VAL(i));
  }
  Value value;
  volatile int hits = 0;
  for (int i = 0; i < count; i++) {
    if (tableGet(&table, keys[i], &value)) hits++;
  }
  double end = nowSeconds();
  if (hits != count) printf("unexpected collision misses: %d\n", count - hits);

  FREE_ARRAY(Value, keys, count);
  freeTable(&table);
  BenchmarkResult result = {"collision_heavy", count * 2, end - start};
  return result;
}

static void printResult(BenchmarkResult result) {
  double opsPerSecond = result.operations / result.seconds;
  double nsPerOp = (result.seconds * 1e9) / result.operations;
  printf("%-26s,%10d,%12.6f,%15.2f,%12.2f\n",
         result.name,
         result.operations,
         result.seconds,
         opsPerSecond,
         nsPerOp);
}

int main(int argc, const char* argv[]) {
  int count = 100000;
  if (argc > 1) {
    count = atoi(argv[1]);
    if (count <= 0) {
      fprintf(stderr, "usage: %s [operation-count]\n", argv[0]);
      return 1;
    }
  }

  initVM();

  printf("benchmark,operations,seconds,ops_per_second,ns_per_op\n");
  printResult(benchmarkInsertStrings(count));
  printResult(benchmarkLookupHits(count));
  printResult(benchmarkLookupMisses(count));
  printResult(benchmarkDeletionChurn(count));
  printResult(benchmarkMixedKeys(count));
  printResult(benchmarkCollisionHeavy(count < 20000 ? count : 20000));

  freeVM();
  return 0;
}
