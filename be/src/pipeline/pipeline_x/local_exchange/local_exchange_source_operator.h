// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include "pipeline/pipeline_x/dependency.h"
#include "pipeline/pipeline_x/operator.h"

namespace doris::pipeline {

struct LocalExchangeSourceDependency final : public Dependency {
public:
    using SharedState = LocalExchangeSharedState;
    LocalExchangeSourceDependency(int id, int node_id, QueryContext* query_ctx)
            : Dependency(id, node_id, "LocalExchangeSourceDependency", query_ctx) {}
    ~LocalExchangeSourceDependency() override = default;

    void block() override {
        if (((LocalExchangeSharedState*)_shared_state.get())->running_sink_operators == 0) {
            return;
        }
        std::unique_lock<std::mutex> lc(((LocalExchangeSharedState*)_shared_state.get())->le_lock);
        if (((LocalExchangeSharedState*)_shared_state.get())->running_sink_operators == 0) {
            return;
        }
        Dependency::block();
    }
};

class LocalExchangeSourceOperatorX;
class LocalExchangeSourceLocalState final
        : public PipelineXLocalState<LocalExchangeSourceDependency> {
public:
    using Base = PipelineXLocalState<LocalExchangeSourceDependency>;
    ENABLE_FACTORY_CREATOR(LocalExchangeSourceLocalState);
    LocalExchangeSourceLocalState(RuntimeState* state, OperatorXBase* parent)
            : Base(state, parent) {}

    Status init(RuntimeState* state, LocalStateInfo& info) override;

private:
    friend class LocalExchangeSourceOperatorX;

    int _channel_id;
    RuntimeProfile::Counter* _get_block_failed_counter = nullptr;
    RuntimeProfile::Counter* _copy_data_timer = nullptr;
};

class LocalExchangeSourceOperatorX final : public OperatorX<LocalExchangeSourceLocalState> {
public:
    using Base = OperatorX<LocalExchangeSourceLocalState>;
    LocalExchangeSourceOperatorX(ObjectPool* pool, int id, OperatorXBase* parent)
            : Base(pool, -1, id), _parent(parent) {}
    Status init(const TPlanNode& tnode, RuntimeState* state) override {
        _op_name = "LOCAL_EXCHANGE_OPERATOR";
        return Status::OK();
    }
    Status prepare(RuntimeState* state) override { return Status::OK(); }
    Status open(RuntimeState* state) override { return Status::OK(); }
    const RowDescriptor& intermediate_row_desc() const override {
        return _child_x->intermediate_row_desc();
    }
    RowDescriptor& row_descriptor() override { return _child_x->row_descriptor(); }
    const RowDescriptor& row_desc() override { return _child_x->row_desc(); }

    Status get_block(RuntimeState* state, vectorized::Block* block,
                     SourceState& source_state) override;

    bool is_source() const override { return true; }

    Status set_child(OperatorXPtr child) override {
        if (_child_x) {
            // Set build side child for join probe operator
            DCHECK(_parent != nullptr);
            RETURN_IF_ERROR(_parent->set_child(child));
        } else {
            _child_x = std::move(child);
        }
        return Status::OK();
    }

private:
    friend class LocalExchangeSourceLocalState;

    OperatorXBase* _parent = nullptr;
};

} // namespace doris::pipeline
