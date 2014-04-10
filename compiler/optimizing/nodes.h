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

#ifndef ART_COMPILER_OPTIMIZING_NODES_H_
#define ART_COMPILER_OPTIMIZING_NODES_H_

#include "utils/allocation.h"
#include "utils/growable_array.h"
#include "dex/arena_bit_vector.h"

namespace art {

class HBasicBlock;
class HInstruction;
class HGraphVisitor;

static const int kDefaultNumberOfBlocks = 8;
static const int kDefaultNumberOfSuccessors = 2;
static const int kDefaultNumberOfPredecessors = 2;
static const int kDefaultNumberOfBackEdges = 1;

// Control-flow graph of a method. Contains a list of basic blocks.
class HGraph : public ArenaObject {
 public:
  explicit HGraph(ArenaAllocator* arena)
      : arena_(arena),
        blocks_(arena, kDefaultNumberOfBlocks),
        dominator_order_(arena, kDefaultNumberOfBlocks) { }

  ArenaAllocator* arena() const { return arena_; }
  const GrowableArray<HBasicBlock*>* blocks() const { return &blocks_; }

  void AddBlock(HBasicBlock* block);
  void BuildDominatorTree();

 private:
  HBasicBlock* FindCommonDominator(HBasicBlock* first, HBasicBlock* second) const;
  void VisitBlockForDominatorTree(HBasicBlock* block,
                                  HBasicBlock* predecessor,
                                  GrowableArray<size_t>* visits);
  void FindBackEdges(ArenaBitVector* visited) const;
  void VisitBlockForBackEdges(HBasicBlock* block,
                              ArenaBitVector* visited,
                              ArenaBitVector* visiting) const;
  void RemoveDeadBlocks(const ArenaBitVector& visited) const;

  HBasicBlock* GetEntryBlock() const { return blocks_.Get(0); }

  ArenaAllocator* const arena_;

  // List of blocks in insertion order.
  GrowableArray<HBasicBlock*> blocks_;

  // List of blocks to perform a pre-order dominator tree traversal.
  GrowableArray<HBasicBlock*> dominator_order_;

  DISALLOW_COPY_AND_ASSIGN(HGraph);
};

class HLoopInformation : public ArenaObject {
 public:
  HLoopInformation(HBasicBlock* header, HGraph* graph)
      : header_(header),
        back_edges_(graph->arena(), kDefaultNumberOfBackEdges) { }

  void AddBackEdge(HBasicBlock* back_edge) {
    back_edges_.Add(back_edge);
  }

  int NumberOfBackEdges() const {
    return back_edges_.Size();
  }

 private:
  HBasicBlock* header_;
  GrowableArray<HBasicBlock*> back_edges_;

  DISALLOW_COPY_AND_ASSIGN(HLoopInformation);
};

// A block in a method. Contains the list of instructions represented
// as a double linked list. Each block knows its predecessors and
// successors.
class HBasicBlock : public ArenaObject {
 public:
  explicit HBasicBlock(HGraph* graph)
      : graph_(graph),
        predecessors_(graph->arena(), kDefaultNumberOfPredecessors),
        successors_(graph->arena(), kDefaultNumberOfSuccessors),
        first_instruction_(nullptr),
        last_instruction_(nullptr),
        loop_information_(nullptr),
        dominator_(nullptr),
        block_id_(-1) { }

  const GrowableArray<HBasicBlock*>* predecessors() const {
    return &predecessors_;
  }

  const GrowableArray<HBasicBlock*>* successors() const {
    return &successors_;
  }

  void AddBackEdge(HBasicBlock* back_edge) {
    if (loop_information_ == nullptr) {
      loop_information_ = new (graph_->arena()) HLoopInformation(this, graph_);
    }
    loop_information_->AddBackEdge(back_edge);
  }

  HGraph* graph() const { return graph_; }

  int block_id() const { return block_id_; }
  void set_block_id(int id) { block_id_ = id; }

  HBasicBlock* dominator() const { return dominator_; }
  void set_dominator(HBasicBlock* dominator) { dominator_ = dominator; }

  int NumberOfBackEdges() const {
    return loop_information_ == nullptr
        ? 0
        : loop_information_->NumberOfBackEdges();
  }

  HInstruction* first_instruction() const { return first_instruction_; }
  HInstruction* last_instruction() const { return last_instruction_; }

  void AddSuccessor(HBasicBlock* block) {
    successors_.Add(block);
    block->predecessors_.Add(this);
  }

  void RemovePredecessor(HBasicBlock* block) {
    predecessors_.Delete(block);
  }

  void AddInstruction(HInstruction* instruction);

 private:
  HGraph* const graph_;
  GrowableArray<HBasicBlock*> predecessors_;
  GrowableArray<HBasicBlock*> successors_;
  HInstruction* first_instruction_;
  HInstruction* last_instruction_;
  HLoopInformation* loop_information_;
  HBasicBlock* dominator_;
  int block_id_;

  DISALLOW_COPY_AND_ASSIGN(HBasicBlock);
};

#define FOR_EACH_INSTRUCTION(M)                            \
  M(Add)                                                   \
  M(Equal)                                                 \
  M(Exit)                                                  \
  M(Goto)                                                  \
  M(If)                                                    \
  M(IntConstant)                                           \
  M(InvokeStatic)                                          \
  M(LoadLocal)                                             \
  M(Local)                                                 \
  M(Return)                                                \
  M(ReturnVoid)                                            \

#define DECLARE_INSTRUCTION(type)                          \
  virtual void Accept(HGraphVisitor* visitor);             \
  virtual const char* DebugName() const { return #type; }  \

class HInstruction : public ArenaObject {
 public:
  HInstruction() : previous_(nullptr), next_(nullptr) { }
  virtual ~HInstruction() { }

  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  virtual intptr_t InputCount() const  = 0;
  virtual HInstruction* InputAt(intptr_t i) const = 0;

  virtual void Accept(HGraphVisitor* visitor) = 0;
  virtual const char* DebugName() const = 0;

 private:
  HInstruction* previous_;
  HInstruction* next_;

  friend class HBasicBlock;

  DISALLOW_COPY_AND_ASSIGN(HInstruction);
};

class HInstructionIterator : public ValueObject {
 public:
  explicit HInstructionIterator(HBasicBlock* block)
      : instruction_(block->first_instruction()) {
    next_ = Done() ? nullptr : instruction_->next();
  }

  inline bool Done() const { return instruction_ == nullptr; }
  inline HInstruction* Current() { return instruction_; }
  inline void Advance() {
    instruction_ = next_;
    next_ = Done() ? nullptr : instruction_->next();
  }

 private:
  HInstruction* instruction_;
  HInstruction* next_;
};

// An embedded container with N elements of type T.  Used (with partial
// specialization for N=0) because embedded arrays cannot have size 0.
template<typename T, intptr_t N>
class EmbeddedArray {
 public:
  EmbeddedArray() : elements_() { }

  intptr_t length() const { return N; }

  const T& operator[](intptr_t i) const {
    ASSERT(i < length());
    return elements_[i];
  }

  T& operator[](intptr_t i) {
    ASSERT(i < length());
    return elements_[i];
  }

  const T& At(intptr_t i) const {
    return (*this)[i];
  }

  void SetAt(intptr_t i, const T& val) {
    (*this)[i] = val;
  }

 private:
  T elements_[N];
};

template<typename T>
class EmbeddedArray<T, 0> {
 public:
  intptr_t length() const { return 0; }
  const T& operator[](intptr_t i) const {
    LOG(FATAL) << "Unreachable";
    static T sentinel = 0;
    return sentinel;
  }
  T& operator[](intptr_t i) {
    LOG(FATAL) << "Unreachable";
    static T sentinel = 0;
    return sentinel;
  }
};

template<intptr_t N>
class HTemplateInstruction: public HInstruction {
 public:
  HTemplateInstruction<N>() : inputs_() { }
  virtual ~HTemplateInstruction() { }

  virtual intptr_t InputCount() const { return N; }
  virtual HInstruction* InputAt(intptr_t i) const { return inputs_[i]; }

 private:
  EmbeddedArray<HInstruction*, N> inputs_;
};

// Represents dex's RETURN_VOID opcode. A HReturnVoid is a control flow
// instruction that branches to the exit block.
class HReturnVoid : public HTemplateInstruction<0> {
 public:
  HReturnVoid() { }

  DECLARE_INSTRUCTION(ReturnVoid)

 private:
  DISALLOW_COPY_AND_ASSIGN(HReturnVoid);
};

// The exit instruction is the only instruction of the exit block.
// Instructions aborting the method (HTrow and HReturn) must branch to the
// exit block.
class HExit : public HTemplateInstruction<0> {
 public:
  HExit() { }

  DECLARE_INSTRUCTION(Exit)

 private:
  DISALLOW_COPY_AND_ASSIGN(HExit);
};

// Jumps from one block to another.
class HGoto : public HTemplateInstruction<0> {
 public:
  HGoto() { }

  DECLARE_INSTRUCTION(Goto)

 private:
  DISALLOW_COPY_AND_ASSIGN(HGoto);
};

// Conditional branch. A block ending with an HIf instruction must have
// two successors.
// TODO: Make it take an input.
class HIf : public HTemplateInstruction<0> {
 public:
  HIf() { }

  DECLARE_INSTRUCTION(If)

 private:
  DISALLOW_COPY_AND_ASSIGN(HIf);
};

class HBinaryOperation : public HTemplateInstruction<2> {
 public:
  HBinaryOperation(Primitive::Type result_type,
                   HInstruction* left,
                   HInstruction* right) : result_type_(result_type) {
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }

  HInstruction* GetLeft() const { return InputAt(0); }
  HInstruction* GetRight() const { return InputAt(1); }
  Primitive::Type GetResultType() const { return result_type_; }

  virtual bool IsCommutative() { return false; }

 private:
  const Primitive::Type result_type_;

  DISALLOW_COPY_AND_ASSIGN(HBinaryOperation);
};


// Instruction to check if two inputs are equal to each other.
class HEqual : public HBinaryOperation {
 public:
  HEqual(HInstruction* first, HInstruction* second)
      : HBinaryOperation(Primitive::kPrimBoolean, first, second) {}

  virtual bool IsCommutative() { return true; }

  DECLARE_INSTRUCTION(Equal)

 private:
  DISALLOW_COPY_AND_ASSIGN(HEqual);
};

// A local in the graph. Corresponds to a Dex register.
class HLocal : public HTemplateInstruction<0> {
 public:
  explicit HLocal(uint16_t reg_number) : reg_number_(reg_number) { }

  DECLARE_INSTRUCTION(Local)

  uint16_t GetRegNumber() const { return reg_number_; }

 private:
  // The Dex register number.
  const uint16_t reg_number_;

  DISALLOW_COPY_AND_ASSIGN(HLocal);
};

// Load a given local. The local is an input of this instruction.
class HLoadLocal : public HTemplateInstruction<1> {
 public:
  explicit HLoadLocal(HLocal* local) {
    SetRawInputAt(0, local);
  }

  HLocal* GetLocal() const { return reinterpret_cast<HLocal*>(InputAt(0)); }

  DECLARE_INSTRUCTION(LoadLocal)

 private:
  DISALLOW_COPY_AND_ASSIGN(HLoadLocal);
};

// Store a value in a given local. This instruction has two inputs: the value
// and the local.
class HStoreLocal : public HTemplateInstruction<2> {
 public:
  HStoreLocal(HLocal* local, HInstruction* value) {
    SetRawInputAt(0, local);
    SetRawInputAt(1, value);
  }

  HLocal* GetLocal() const { return reinterpret_cast<HLocal*>(InputAt(0)); }

  DECLARE_INSTRUCTION(StoreLocal)

 private:
  DISALLOW_COPY_AND_ASSIGN(HStoreLocal);
};

// Constants of the type int. Those can be from Dex instructions, or
// synthesized (for example with the if-eqz instruction).
class HIntConstant : public HTemplateInstruction<0> {
 public:
  explicit HIntConstant(int32_t value) : value_(value) { }

  int32_t GetValue() const { return value_; }

  DECLARE_INSTRUCTION(IntConstant)

 private:
  const int32_t value_;

  DISALLOW_COPY_AND_ASSIGN(HIntConstant);
};

class HInvoke : public HInstruction {
 public:
  HInvoke(ArenaAllocator* arena, uint32_t number_of_arguments, int32_t dex_pc)
    : inputs_(arena, number_of_arguments),
      dex_pc_(dex_pc) {
    inputs_.SetSize(number_of_arguments);
  }

  virtual intptr_t InputCount() const { return inputs_.Size(); }
  virtual HInstruction* InputAt(intptr_t i) const { return inputs_.Get(i); }

  int32_t GetDexPc() const { return dex_pc_; }

 protected:
  GrowableArray<HInstruction*> inputs_;
  const int32_t dex_pc_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HInvoke);
};

class HInvokeStatic : public HInvoke {
 public:
  HInvokeStatic(ArenaAllocator* arena, uint32_t number_of_arguments, int32_t dex_pc, int32_t index_in_dex_cache)
      : HInvoke(arena, number_of_arguments, dex_pc), index_in_dex_cache_(index_in_dex_cache) { }

  uint32_t GetIndexInDexCache() const { return index_in_dex_cache_; }

  DECLARE_INSTRUCTION(InvokeStatic)

 private:
  uint32_t index_in_dex_cache_;

  DISALLOW_COPY_AND_ASSIGN(HInvokeStatic);
};

class HAdd : public HBinaryOperation {
 public:
  HAdd(Primitive::Type result_type, HInstruction* left, HInstruction* right)
      : HBinaryOperation(result_type, left, right) {}

  virtual bool IsCommutative() { return true; }

  DECLARE_INSTRUCTION(Add);

 private:
  DISALLOW_COPY_AND_ASSIGN(HAdd);
};

class HGraphVisitor : public ValueObject {
 public:
  explicit HGraphVisitor(HGraph* graph) : graph_(graph) { }
  virtual ~HGraphVisitor() { }

  virtual void VisitInstruction(HInstruction* instruction) { }
  virtual void VisitBasicBlock(HBasicBlock* block);

  void VisitInsertionOrder();

  // Visit functions for instruction classes.
#define DECLARE_VISIT_INSTRUCTION(name)                                        \
  virtual void Visit##name(H##name* instr) { VisitInstruction(instr); }

  FOR_EACH_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

 private:
  HGraph* graph_;

  DISALLOW_COPY_AND_ASSIGN(HGraphVisitor);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_H_
