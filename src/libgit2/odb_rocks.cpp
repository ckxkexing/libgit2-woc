//
// Created by 15857 on 2022/11/28.
//
/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include <string>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <rocksdb/db.h>
#include <rocksdb/c.h>
#include <rocksdb/slice.h>
extern "C"
{
	#include <git2.h>
	#include <git2/sys/odb_backend.h>
	#include <git2/odb_backend.h>
}

struct rocks_backend {
	git_odb_backend parent;

	rocksdb::DB* db;
	rocksdb::Options opt;
	rocksdb::Status status;
};

static const char *type_suffix {":type"};
static const char *size_suffix {":size"};
static const char *data_suffix {":data"};

void rocks_backend__build_key(std::string & newKey, const unsigned char *id, const char *suffix) {
	newKey.resize(20 + strlen(suffix));
	memcpy(newKey.begin().base(), id, 20);
	memcpy(newKey.begin().base() + 20, suffix, strlen(suffix));
}

int rocks_backend__read_header(size_t *len_p, git_otype *type_p, git_odb_backend *_backend, const git_oid *oid) {

	assert(len_p && type_p && _backend && oid);

	rocks_backend *backend = (rocks_backend*)(_backend);
	std::string type_key, size_key;

	rocks_backend__build_key(type_key, oid->id, type_suffix);
	rocks_backend__build_key(size_key, oid->id, size_suffix);

	std::string type_value, size_value;
	auto s = backend->db->Get(rocksdb::ReadOptions(), type_key, &type_value);
	if(!s.ok())
		return GIT_ENOTFOUND;

	s = backend->db->Get(rocksdb::ReadOptions(), size_key, &size_value);
	if(!s.ok())
		return GIT_ENOTFOUND;

	*type_p = static_cast<git_otype>(std::stoi(type_value));
	*len_p = std::stoll(size_value);

	return GIT_OK;
}

int rocks_backend__read(void **data_p, size_t *len_p, git_otype *type_p, git_odb_backend *_backend, const git_oid *oid) {
    char shortsha[41] = {0};
    git_oid_tostr(shortsha, 41, oid);
    printf("read from rocksdb %s\n", shortsha);
	assert(data_p && len_p && type_p && _backend && oid);
	rocks_backend *backend = (rocks_backend*)(_backend);

	std::string type_key, key_value;
	rocks_backend__build_key(type_key, oid->id, type_suffix);
	auto s = backend->db->Get(rocksdb::ReadOptions(), type_key, &key_value);
	if(!s.ok())
		return GIT_ENOTFOUND;
	std::string data_key, data_value;
	rocks_backend__build_key(data_key, oid->id, data_suffix);
	s = backend->db->Get(rocksdb::ReadOptions(), data_key, &data_value);
	if(!s.ok())
		return GIT_ENOTFOUND;
    std::string size_key, size_value;
    rocks_backend__build_key(size_key, oid->id, size_suffix);
    s = backend->db->Get(rocksdb::ReadOptions(), size_key, &size_value);
    if(!s.ok())
        return GIT_ENOTFOUND;

    size_t data_size = std::stoll(size_value);
    *data_p = malloc(data_size);
    std::memcpy(*data_p, data_value.c_str(), data_size);
    *type_p = (git_otype) atoi(key_value.c_str());
	*len_p = std::stoll(size_value);
	return GIT_OK;
}

int rocks_backend__exists(git_odb_backend *_backend, const git_oid *oid) {
    printf("come rocks to check?\n");
	assert(_backend && oid);
	std::string key;
	rocks_backend__build_key(key, oid->id, type_suffix);
	std::string value;
	rocks_backend *backend = (rocks_backend*)(_backend);
	rocksdb::Status s = backend->db->Get(rocksdb::ReadOptions(), key, &value);
    printf("and result = %d\n", s.ok());
	return (s.ok() ? 1 : 0);
}

static int rocks_backend__foreach(git_odb_backend *_backend, git_odb_foreach_cb cb, void *data)
{
    // 因为我想从pack中
    // 将数据全部读取到rocksdb中
    // 而不从rocks中读取数据
    // 所以这里的foreach
    // 可以先假装为空。
    int error;
    struct pack_backend *backend;
    unsigned int i;
    return 0;
}

int rocks_backend__write(git_odb_backend *_backend, const git_oid *oid, const void *data, size_t len, git_otype type) {
	assert(oid && _backend && data);

	rocks_backend *backend = (rocks_backend*)(_backend);
	if(git_odb_hash((git_oid*)oid, data, len, type) < 0)
		return GIT_ERROR;

	std::string type_key, size_key, data_key;
	rocks_backend__build_key(type_key, oid->id, type_suffix);
	rocks_backend__build_key(size_key, oid->id, size_suffix);
	rocks_backend__build_key(data_key, oid->id, data_suffix);

	rocksdb::Status s = backend->db->Put(rocksdb::WriteOptions(), type_key, std::to_string(type));
	if(!s.ok())
		return GIT_ERROR;

	s = backend->db->Put(rocksdb::WriteOptions(), size_key, std::to_string(len));
	if(!s.ok())
		return GIT_ERROR;
    rocksdb::Slice d = rocksdb::Slice((const char*)data, len);
	s = backend->db->Put(rocksdb::WriteOptions(), data_key, d);
	if(!s.ok())
		return GIT_ERROR;
	return GIT_OK;
}

void rocks_backend__free(git_odb_backend *_backend) {
	auto backend = (rocks_backend*)(_backend);
	delete backend->db;
	delete _backend;
}

int git_odb_backend_rocks(git_odb_backend **backend_out,  const char *rocksPath) {

	rocks_backend *backend{nullptr};
	backend = new rocks_backend();

	backend->opt.create_if_missing = true;
	backend->status = rocksdb::DB::Open(backend->opt, rocksPath, &backend->db);
	if(!backend->status.ok()) {
		rocks_backend__free((git_odb_backend *)backend);
		return GIT_ERROR;
	}
	backend->parent.version = 1; // GIT_ODB_BACKEND_VERSION;
	backend->parent.read = &rocks_backend__read;
	backend->parent.read_header = &rocks_backend__read_header;
	backend->parent.write = &rocks_backend__write;
	backend->parent.exists = &rocks_backend__exists;
	backend->parent.free = &rocks_backend__free;
    backend->parent.foreach = &rocks_backend__foreach;
	*backend_out = (git_odb_backend *)backend;
	return GIT_OK;
}