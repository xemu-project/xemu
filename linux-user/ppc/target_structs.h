/*
 * PowerPC specific structures for linux-user
 *
 * Copyright (c) 2013 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef PPC_TARGET_STRUCTS_H
#define PPC_TARGET_STRUCTS_H

struct target_ipc_perm {
    abi_int __key;                      /* Key.  */
    abi_uint uid;                       /* Owner's user ID.  */
    abi_uint gid;                       /* Owner's group ID.  */
    abi_uint cuid;                      /* Creator's user ID.  */
    abi_uint cgid;                      /* Creator's group ID.  */
    abi_uint mode;                      /* Read/write permission.  */
    uint32_t __seq;                     /* Sequence number.  */
    uint32_t __pad1;
    uint64_t __unused1;
    uint64_t __unused2;
};

struct target_shmid_ds {
    struct target_ipc_perm shm_perm;    /* operation permission struct */
#if TARGET_ABI_BITS == 32
    abi_uint __unused1;
#endif
    abi_ulong shm_atime;                /* time of last shmat() */
#if TARGET_ABI_BITS == 32
    abi_uint __unused2;
#endif
    abi_ulong shm_dtime;                /* time of last shmdt() */
#if TARGET_ABI_BITS == 32
    abi_uint __unused3;
#endif
    abi_ulong shm_ctime;                /* time of last change by shmctl() */
#if TARGET_ABI_BITS == 32
    abi_uint __unused4;
#endif
    abi_long shm_segsz;                 /* size of segment in bytes */
    abi_int shm_cpid;                   /* pid of creator */
    abi_int shm_lpid;                   /* pid of last shmop */
    abi_ulong shm_nattch;               /* number of current attaches */
    abi_ulong __unused5;
    abi_ulong __unused6;
};

#endif
