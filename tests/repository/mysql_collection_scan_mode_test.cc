/**
 *   Copyright 2021 Alibaba, Inc. and its affiliates. All Rights Reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.

 *   \author   Dianzhang.Chen
 *   \date     Dec 2020
 *   \brief
 */

#include <gmock/gmock.h>


#define private public
#define protected public
#include "repository/repository_common/config.h"
#undef private
#undef protected

#include "repository/mysql_collection.h"
#include "mock_index_agent_server.h"
#include "mock_mysql_handler.h"
#include "port_helper.h"

const std::string collection_name = "mysql_collection_test.info";
static int PORT = 8010;
static int PID = 0;

////////////////////////////////////////////////////////////////////
class MysqlCollectionScanTest2 : public ::testing::Test {
 protected:
  MysqlCollectionScanTest2() {}
  ~MysqlCollectionScanTest2() {}
  void SetUp() {
    PortHelper::GetPort(&PORT, &PID);
    std::cout << "Server port: " << PORT << std::endl;
    std::string index_uri = "127.0.0.1:" + std::to_string(PORT);
    proxima::be::repository::Config::Instance()
        .repository_config_.mutable_repository_config()
        ->set_index_agent_addr(index_uri);
    std::cout << "Set index addr: " << index_uri << std::endl;

    proxima::be::repository::Config::Instance()
        .repository_config_.mutable_repository_config()
        ->set_batch_interval(1000000);
    std::cout << "Set batch_interval to 1s" << std::endl;
  }
  void TearDown() {
    PortHelper::RemovePortFile(PID);
  }
};

TEST_F(MysqlCollectionScanTest2, TestGeneral) {
  brpc::Server server_;
  MockGeneralProximaServiceImpl svc_;
  brpc::ServerOptions options;
  ASSERT_EQ(0, server_.AddService(&svc_, brpc::SERVER_DOESNT_OWN_SERVICE));
  ASSERT_EQ(0, server_.Start(PORT, &options));
  {
    proto::CollectionConfig config;
    config.set_collection_name(collection_name);

    CollectionPtr collection{nullptr};
    MockMysqlHandlerPtr mysql_handler =
        std::make_shared<MockMysqlHandler>(config);
    EXPECT_CALL(*mysql_handler, init(_))
        .WillRepeatedly(Return(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*mysql_handler, start(_))
        .WillRepeatedly(Return(0))
        .RetiresOnSaturation();

    EXPECT_CALL(*mysql_handler,
                get_next_row_data(Matcher<proto::WriteRequest::Row *>(_),
                                  Matcher<LsnContext *>(_)))
        .WillOnce(Invoke(
            [](proto::WriteRequest::Row *row_data, LsnContext *context) -> int {
              row_data->set_primary_key(1);
              row_data->mutable_lsn_context()->set_lsn(1);
              context->status = RowDataStatus::NORMAL;
              return 0;
            }))
        .WillOnce(Invoke(
            [](proto::WriteRequest::Row *row_data, LsnContext *context) -> int {
              row_data->set_primary_key(2);
              row_data->mutable_lsn_context()->set_lsn(2);
              context->status = RowDataStatus::NORMAL;
              return 0;
            }))
        .WillOnce(Invoke(
            [](proto::WriteRequest::Row *row_data, LsnContext *context) -> int {
              row_data->set_primary_key(3);
              context->status = RowDataStatus::NO_MORE_DATA;
              return 0;
            }))
        .WillOnce(Invoke(
            [](proto::WriteRequest::Row *row_data, LsnContext *context) -> int {
              row_data->set_primary_key(3);
              row_data->mutable_lsn_context()->set_lsn(3);
              context->status = RowDataStatus::NORMAL;
              return 0;
            }))
        .WillOnce(Invoke(
            [](proto::WriteRequest::Row *row_data, LsnContext *context) -> int {
              row_data->set_primary_key(4);
              row_data->mutable_lsn_context()->set_lsn(4);
              context->status = RowDataStatus::NORMAL;
              return 0;
            }))
        .WillRepeatedly(Invoke(
            [](proto::WriteRequest::Row *row_data, LsnContext *context) -> int {
              row_data->set_primary_key(3);
              context->status = RowDataStatus::SCHEMA_CHANGED;
              return 0;
            }))
        .RetiresOnSaturation();

    EXPECT_CALL(*mysql_handler,
                reset_status(Matcher<ScanMode>(_),
                             Matcher<const proto::CollectionConfig &>(_),
                             Matcher<const LsnContext &>(_)))
        // EXPECT_CALL(*mysql_handler, reset_status(_, _, _))
        .WillRepeatedly(Return(0))
        .RetiresOnSaturation();

    EXPECT_CALL(*mysql_handler, get_fields_meta(_))
        .WillRepeatedly(Invoke([](proto::WriteRequest::RowMeta *meta) -> int {
          meta->add_index_column_metas()->set_column_name("index1");
          meta->add_index_column_metas()->set_column_name("index2");
          meta->add_forward_column_names("forward1");
          meta->add_forward_column_names("forward2");
          return 0;
        }))
        .RetiresOnSaturation();

    EXPECT_CALL(*mysql_handler, get_table_snapshot(_, _))
        .WillRepeatedly(Return(0))
        .RetiresOnSaturation();

    collection.reset(new (std::nothrow) MysqlCollection(config, mysql_handler));

    int ret = collection->init();
    ASSERT_EQ(ret, 0);
    CollectionStatus current_state = collection->state();
    ASSERT_EQ(current_state, CollectionStatus::INIT);
    collection->run();
    sleep(1);

    // check value
    ASSERT_EQ(svc_.get_server_called_count(), 2);
    proto::WriteRequest request;
    std::string request_str = svc_.get_request_string(0);
    ASSERT_EQ(request_str.empty(), false);
    ret = request.ParseFromString(request_str);

    ASSERT_EQ(ret, true);
    auto rows = request.rows();
    ASSERT_EQ(rows.size(), 2);
    auto row1 = rows[0];
    auto row2 = rows[1];
    // row1
    ASSERT_EQ(row1.primary_key(), 1);
    ASSERT_EQ(row1.lsn_context().lsn(), 1);
    // row2
    ASSERT_EQ(row2.primary_key(), 2);
    ASSERT_EQ(row2.lsn_context().lsn(), 2);
    // meta
    auto meta = request.row_meta();
    ASSERT_EQ(meta.index_column_metas_size(), 2);
    auto index_column_metas = meta.index_column_metas();
    ASSERT_EQ(index_column_metas[0].column_name(), "index1");
    ASSERT_EQ(index_column_metas[1].column_name(), "index2");
    ASSERT_EQ(meta.index_column_metas_size(), 2);
    auto forward_column_names = meta.forward_column_names();
    ASSERT_EQ(forward_column_names[0], "forward1");
    ASSERT_EQ(forward_column_names[1], "forward2");

    proto::WriteRequest request2;
    request_str = svc_.get_request_string(1);
    ASSERT_EQ(request_str.empty(), false);
    ret = request.ParseFromString(request_str);

    ASSERT_EQ(ret, true);
    rows = request.rows();
    ASSERT_EQ(rows.size(), 2);
    row1 = rows[0];
    row2 = rows[1];
    // row1
    ASSERT_EQ(row1.primary_key(), 3);
    ASSERT_EQ(row1.lsn_context().lsn(), 3);
    // row2
    ASSERT_EQ(row2.primary_key(), 4);
    ASSERT_EQ(row2.lsn_context().lsn(), 4);
    // meta
    meta = request.row_meta();
    ASSERT_EQ(meta.index_column_metas_size(), 2);
    index_column_metas = meta.index_column_metas();
    ASSERT_EQ(index_column_metas[0].column_name(), "index1");
    ASSERT_EQ(index_column_metas[1].column_name(), "index2");
    ASSERT_EQ(meta.index_column_metas_size(), 2);
    forward_column_names = meta.forward_column_names();
    ASSERT_EQ(forward_column_names[0], "forward1");
    ASSERT_EQ(forward_column_names[1], "forward2");
    // exit
    collection->stop();
    sleep(2);
    LOG_INFO("MysqlCollectionTest2::TestGeneral PASS");
  }

  ASSERT_EQ(0, server_.Stop(0));
  ASSERT_EQ(0, server_.Join());
}
