// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_message_test_sink.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace proxy {

namespace {

// Backend for GetFirstResource[Call|Reply]Matching.
template<class WrapperMessage, class Params>
bool GetFirstResourceMessageMatching(const ResourceMessageTestSink& sink,
                                     uint32 id,
                                     Params* params,
                                     IPC::Message* nested_msg) {
  for (size_t i = 0; i < sink.message_count(); i++) {
    const IPC::Message* msg = sink.GetMessageAt(i);
    if (msg->type() == WrapperMessage::ID) {
      Params cur_params;
      IPC::Message cur_msg;
      WrapperMessage::Read(msg, &cur_params, &cur_msg);
      if (cur_msg.type() == id) {
        *params = cur_params;
        *nested_msg = cur_msg;
        return true;
      }
    }
  }
  return false;
}

}  // namespace

ResourceMessageTestSink::ResourceMessageTestSink() {
}

ResourceMessageTestSink::~ResourceMessageTestSink() {
}

bool ResourceMessageTestSink::Send(IPC::Message* msg) {
  int message_id = 0;
  scoped_ptr<IPC::MessageReplyDeserializer> reply_deserializer;
  if (msg->is_sync()) {
    reply_deserializer.reset(
        static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer());
    message_id = IPC::SyncMessage::GetMessageId(*msg);
  }
  bool result = IPC::TestSink::Send(msg);  // Deletes |msg|.
  if (sync_reply_msg_.get()) {
    // |sync_reply_msg_| should always be a reply to the pending sync message.
    DCHECK(IPC::SyncMessage::IsMessageReplyTo(*sync_reply_msg_.get(),
                                              message_id));
    reply_deserializer->SerializeOutputParameters(*sync_reply_msg_.get());
    sync_reply_msg_.reset(NULL);
  }
  return result;
}

void ResourceMessageTestSink::SetSyncReplyMessage(IPC::Message* reply_msg) {
  DCHECK(!sync_reply_msg_.get());
  sync_reply_msg_.reset(reply_msg);
}

bool ResourceMessageTestSink::GetFirstResourceCallMatching(
    uint32 id,
    ResourceMessageCallParams* params,
    IPC::Message* nested_msg) const {
  return GetFirstResourceMessageMatching<PpapiHostMsg_ResourceCall,
                                         ResourceMessageCallParams>(
      *this, id, params, nested_msg);
}

bool ResourceMessageTestSink::GetFirstResourceReplyMatching(
    uint32 id,
    ResourceMessageReplyParams* params,
    IPC::Message* nested_msg) {
  return GetFirstResourceMessageMatching<PpapiPluginMsg_ResourceReply,
                                         ResourceMessageReplyParams>(
      *this, id, params, nested_msg);
}

ResourceSyncCallHandler::ResourceSyncCallHandler(
    ResourceMessageTestSink* test_sink,
    uint32 incoming_type,
    int32_t result,
    const IPC::Message& reply_msg)
    : test_sink_(test_sink),
      incoming_type_(incoming_type),
      result_(result),
      reply_msg_(reply_msg) {
}

ResourceSyncCallHandler::~ResourceSyncCallHandler() {
}

bool ResourceSyncCallHandler::OnMessageReceived(const IPC::Message& msg) {
  if (msg.type() != PpapiHostMsg_ResourceSyncCall::ID)
    return false;
  PpapiHostMsg_ResourceSyncCall::Schema::SendParam send_params;
  bool success = PpapiHostMsg_ResourceSyncCall::ReadSendParam(
      &msg, &send_params);
  DCHECK(success);
  ResourceMessageCallParams call_params = send_params.a;
  IPC::Message call_msg = send_params.b;
  if (call_msg.type() != incoming_type_)
    return false;
  IPC::Message* wrapper_reply_msg = IPC::SyncMessage::GenerateReply(&msg);
  ResourceMessageReplyParams reply_params(call_params.pp_resource(),
                                          call_params.sequence());
  reply_params.set_result(result_);
  PpapiHostMsg_ResourceSyncCall::WriteReplyParams(
      wrapper_reply_msg, reply_params, reply_msg_);
  test_sink_->SetSyncReplyMessage(wrapper_reply_msg);

  // Stash a copy of the message for inspection later.
  last_handled_msg_ = call_msg;
  return true;
}

}  // namespace proxy
}  // namespace ppapi
