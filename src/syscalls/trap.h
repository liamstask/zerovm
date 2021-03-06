/*
 * trap.h
 *
 * Copyright (c) 2012, LiteStack, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TRAP_H_
#define TRAP_H_

#include "src/loader/sel_ldr.h"
#include "src/channels/mount_channel.h"

EXTERN_C_BEGIN

/*
 * jump/call alignement of untrusted code. will prevent jumping into
 * the middle of instruction and execution of uncontrolled code sequence
 */
#define OP_ALIGNEMENT 0x20

/*
 * single syscall allowed in zerovm. all service available via trap functions
 *
 * 1st parameter is a pointer to the command (function, arg1, argv2,..)
 * 2nd parameter is a pointer to return value(s)
 * return 0 if successful, otherwise -errno
 *
 * notice about args: since nacl patches two 1st arguments if they are pointers,
 * arg[1] should not be used
 */
int32_t TrapHandler(struct NaClApp *nap, uint32_t args);

/* macros use channel type and limits */
#define CHANNEL_READABLE(channel) ((ChannelIOMask(channel) & 1) == 1)
#define CHANNEL_WRITEABLE(channel) ((ChannelIOMask(channel) & 2) == 2)

/* macros use only channel type */
#define CHANNEL_SEQ_READABLE(channel) (channel->type == 0 || channel->type == 2)
#define CHANNEL_SEQ_WRITEABLE(channel) (channel->type == 0 || channel->type == 1)
#define CHANNEL_RND_READABLE(channel) (channel->type == 1 || channel->type == 3)
#define CHANNEL_RND_WRITEABLE(channel) (channel->type == 2 || channel->type == 3)

EXTERN_C_END

#endif /* TRAP_H_ */
