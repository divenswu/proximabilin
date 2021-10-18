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

 *   \author   Haichao.chc
 *   \date     Jun 2021
 *   \brief    ForwardReader load forward index and provides read ability.
 */

#include "forward_reader.h"
#include "simple_forward_reader.h"

namespace proxima {
namespace be {
namespace index {

ForwardReaderPtr ForwardReader::Create(const std::string &collection_name,
                                       const std::string &collection_path,
                                       SegmentID segment_id) {
  return std::make_shared<SimpleForwardReader>(collection_name, collection_path,
                                               segment_id);
}


}  // end namespace index
}  // namespace be
}  // end namespace proxima
