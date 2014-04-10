/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "builder.h"
#include "dex_instruction.h"
#include "nodes.h"
#include "utils/arena_allocator.h"

#include "gtest/gtest.h"

namespace art {

static void TestCode(const uint16_t* data, int length, const int* blocks, size_t blocks_length) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  HGraphBuilder builder(&allocator);
  HGraph* graph = builder.BuildGraph(data, data + length);
  ASSERT_NE(graph, nullptr);
  graph->BuildDominatorTree();
  ASSERT_EQ(graph->blocks()->Size(), blocks_length);
  for (size_t i = 0; i < blocks_length; i++) {
    if (blocks[i] == -1) {
      ASSERT_EQ(nullptr, graph->blocks()->Get(i)->dominator());
    } else {
      ASSERT_NE(nullptr, graph->blocks()->Get(i)->dominator());
      ASSERT_EQ(blocks[i], graph->blocks()->Get(i)->dominator()->block_id());
    }
  }
}

TEST(OptimizerTest, ReturnVoid) {
  const uint16_t data[] = {
      Instruction::RETURN_VOID  // Block number 1
  };

  const int dominators[] = {
    -1,
    0,
    1
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG1) {
  const uint16_t data[] = {
    Instruction::GOTO | 0x100,  // Block number 1
    Instruction::RETURN_VOID  // Block number 2
  };

  const int dominators[] = {
    -1,
    0,
    1,
    2
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG2) {
  const uint16_t data[] = {
    Instruction::GOTO | 0x100,  // Block number 1
    Instruction::GOTO | 0x100,  // Block number 2
    Instruction::RETURN_VOID  // Block number 3
  };

  const int dominators[] = {
    -1,
    0,
    1,
    2,
    3
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG3) {
  const uint16_t data1[] = {
    Instruction::GOTO | 0x200,  // Block number 1
    Instruction::RETURN_VOID,  // Block number 2
    Instruction::GOTO | 0xFF00  // Block number 3
  };
  const int dominators[] = {
    -1,
    0,
    3,
    1,
    2
  };

  TestCode(data1, sizeof(data1) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));

  const uint16_t data2[] = {
    Instruction::GOTO_16, 3,
    Instruction::RETURN_VOID,
    Instruction::GOTO_16, 0xFFFF
  };

  TestCode(data2, sizeof(data2) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));

  const uint16_t data3[] = {
    Instruction::GOTO_32, 4, 0,
    Instruction::RETURN_VOID,
    Instruction::GOTO_32, 0xFFFF, 0xFFFF
  };

  TestCode(data3, sizeof(data3) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG4) {
  const uint16_t data1[] = {
    Instruction::NOP,
    Instruction::GOTO | 0xFF00
  };

  const int dominators[] = {
    -1,
    0,
    -1
  };

  TestCode(data1, sizeof(data1) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));

  const uint16_t data2[] = {
    Instruction::GOTO_32, 0, 0
  };

  TestCode(data2, sizeof(data2) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG5) {
  const uint16_t data[] = {
    Instruction::RETURN_VOID,   // Block number 1
    Instruction::GOTO | 0x100,  // Dead block
    Instruction::GOTO | 0xFE00  // Block number 2
  };

  const int dominators[] = {
    -1,
    0,
    -1,
    1
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG6) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID
  };

  const int dominators[] = {
    -1,
    0,
    1,
    1,
    3
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG7) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,        // Block number 1
    Instruction::GOTO | 0x100,    // Block number 2
    Instruction::GOTO | 0xFF00);  // Block number 3

  const int dominators[] = {
    -1,
    0,
    1,
    1,
    -1  // exit block is not dominated by any block due to the spin loop.
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG8) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,        // Block number 1
    Instruction::GOTO | 0x200,    // Block number 2
    Instruction::GOTO | 0x100,    // Block number 3
    Instruction::GOTO | 0xFF00);  // Block number 4

  const int dominators[] = {
    -1,
    0,
    1,
    1,
    1,
    -1  // exit block is not dominated by any block due to the spin loop.
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG9) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,        // Block number 1
    Instruction::GOTO | 0x200,    // Block number 2
    Instruction::GOTO | 0x100,    // Block number 3
    Instruction::GOTO | 0xFE00);  // Block number 4

  const int dominators[] = {
    -1,
    0,
    1,
    1,
    1,
    -1  // exit block is not dominated by any block due to the spin loop.
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

TEST(OptimizerTest, CFG10) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 6,  // Block number 1
    Instruction::IF_EQ, 3,  // Block number 2
    Instruction::GOTO | 0x100,  // Block number 3
    Instruction::GOTO | 0x100,  // Block number 4
    Instruction::RETURN_VOID  // Block number 5
  };

  const int dominators[] = {
    -1,
    0,
    1,
    2,
    2,
    1,
    5  // Block number 5 dominates exit block
  };

  TestCode(data, sizeof(data) / sizeof(uint16_t), dominators, sizeof(dominators) / sizeof(int));
}

}  // namespace art
