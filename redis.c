/* -*- Mode: C; tab-width: 4 -*- */
/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2009 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Original author: Alfonso Jimenez <yo@alfonsojimenez.com>             |
  | Maintainer: Nicolas Favre-Felix <n.favre-felix@owlient.eu>           |
  | Maintainer: Nasreddine Bouafif <n.bouafif@owlient.eu>                |
  | Maintainer: Michael Grunder <michael.grunder@gmail.com>              |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "ext/standard/info.h"
#include "php_ini.h"
#include "php_redis.h"
#include "redis_commands.h"
#include "redis_array.h"
#include "redis_cluster.h"
#include <zend_exceptions.h>

#ifdef PHP_SESSION
#include "ext/session/php_session.h"
#endif

#include <ext/standard/php_smart_str.h>
#include <ext/standard/php_var.h>
#include <ext/standard/php_math.h>

#include "library.h"

#define R_SUB_CALLBACK_CLASS_TYPE 1
#define R_SUB_CALLBACK_FT_TYPE 2
#define R_SUB_CLOSURE_TYPE 3

int le_redis_sock;
extern int le_redis_array;

#ifdef PHP_SESSION
extern ps_module ps_mod_redis;
#endif

extern zend_class_entry *redis_array_ce;
extern zend_class_entry *redis_cluster_ce;
zend_class_entry *redis_ce;
zend_class_entry *redis_exception_ce;
extern zend_class_entry *redis_cluster_exception_ce;
zend_class_entry *spl_ce_RuntimeException = NULL;

extern zend_function_entry redis_array_functions[];
extern zend_function_entry redis_cluster_functions[];

PHP_INI_BEGIN()
	/* redis arrays */
	PHP_INI_ENTRY("redis.arrays.names", "", PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("redis.arrays.hosts", "", PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("redis.arrays.previous", "", PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("redis.arrays.functions", "", PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("redis.arrays.index", "", PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("redis.arrays.autorehash", "", PHP_INI_ALL, NULL)
PHP_INI_END()

/**
 * Argument info for the SCAN proper
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_scan, 0, 0, 1)
    ZEND_ARG_INFO(1, i_iterator)
    ZEND_ARG_INFO(0, str_pattern)
    ZEND_ARG_INFO(0, i_count)
ZEND_END_ARG_INFO();

/**
 * Argument info for key scanning
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_kscan, 0, 0, 2)
    ZEND_ARG_INFO(0, str_key)
    ZEND_ARG_INFO(1, i_iterator)
    ZEND_ARG_INFO(0, str_pattern)
    ZEND_ARG_INFO(0, i_count)
ZEND_END_ARG_INFO();

#ifdef ZTS
ZEND_DECLARE_MODULE_GLOBALS(redis)
#endif

static zend_function_entry redis_functions[] = {
     PHP_ME(Redis, __construct, NULL, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
     PHP_ME(Redis, __destruct, NULL, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
     PHP_ME(Redis, connect, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pconnect, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, close, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, ping, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, echo, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, get, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, set, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, setex, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, psetex, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, setnx, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getSet, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, randomKey, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, renameKey, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, renameNx, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getMultiple, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, exists, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, delete, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, incr, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, incrBy, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, incrByFloat, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, decr, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, decrBy, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, type, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, append, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getRange, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, setRange, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getBit, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, setBit, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, strlen, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getKeys, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sort, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sortAsc, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sortAscAlpha, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sortDesc, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sortDescAlpha, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lPush, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, rPush, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lPushx, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, rPushx, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lPop, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, rPop, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, blPop, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, brPop, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lSize, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lRemove, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, listTrim, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lGet, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lGetRange, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lSet, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lInsert, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sAdd, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sSize, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sRemove, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sMove, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sPop, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sRandMember, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sContains, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sMembers, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sInter, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sInterStore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sUnion, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sUnionStore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sDiff, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sDiffStore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, setTimeout, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, save, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, bgSave, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, lastSave, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, flushDB, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, flushAll, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, dbSize, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, auth, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, ttl, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pttl, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, persist, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, info, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, select, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, move, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, bgrewriteaof, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, slaveof, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, object, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, bitop, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, bitcount, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, bitpos, NULL, ZEND_ACC_PUBLIC)

     /* 1.1 */
     PHP_ME(Redis, mset, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, msetnx, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, rpoplpush, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, brpoplpush, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zAdd, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zDelete, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zRange, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zRevRange, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zRangeByScore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zRevRangeByScore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zRangeByLex, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zCount, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zDeleteRangeByScore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zDeleteRangeByRank, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zCard, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zScore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zRank, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zRevRank, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zInter, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zUnion, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zIncrBy, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, expireAt, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pexpire, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pexpireAt, NULL, ZEND_ACC_PUBLIC)

     /* 1.2 */
     PHP_ME(Redis, hGet, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hSet, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hSetNx, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hDel, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hLen, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hKeys, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hVals, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hGetAll, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hExists, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hIncrBy, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hIncrByFloat, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hMset, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hMget, NULL, ZEND_ACC_PUBLIC)

     PHP_ME(Redis, multi, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, discard, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, exec, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pipeline, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, watch, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, unwatch, NULL, ZEND_ACC_PUBLIC)

	 PHP_ME(Redis, publish, NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(Redis, subscribe, NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(Redis, psubscribe, NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(Redis, unsubscribe, NULL, ZEND_ACC_PUBLIC)
	 PHP_ME(Redis, punsubscribe, NULL, ZEND_ACC_PUBLIC)

	 PHP_ME(Redis, time, NULL, ZEND_ACC_PUBLIC)

     PHP_ME(Redis, eval, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, evalsha, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, script, NULL, ZEND_ACC_PUBLIC)

     PHP_ME(Redis, debug, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, dump, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, restore, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, migrate, NULL, ZEND_ACC_PUBLIC)

     PHP_ME(Redis, getLastError, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, clearLastError, NULL, ZEND_ACC_PUBLIC)

     PHP_ME(Redis, _prefix, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, _serialize, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, _unserialize, NULL, ZEND_ACC_PUBLIC)

     PHP_ME(Redis, client, NULL, ZEND_ACC_PUBLIC)

     /* SCAN and friends */
     PHP_ME(Redis, scan, arginfo_scan, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, hscan, arginfo_kscan, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, zscan, arginfo_kscan, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, sscan, arginfo_kscan, ZEND_ACC_PUBLIC)

     /* HyperLogLog commands */
     PHP_ME(Redis, pfadd, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pfcount, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pfmerge, NULL, ZEND_ACC_PUBLIC)

     /* options */
     PHP_ME(Redis, getOption, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, setOption, NULL, ZEND_ACC_PUBLIC)

     /* config */
     PHP_ME(Redis, config, NULL, ZEND_ACC_PUBLIC)

     /* slowlog */
     PHP_ME(Redis, slowlog, NULL, ZEND_ACC_PUBLIC)

     /* Send a raw command and read raw results */
     PHP_ME(Redis, rawCommand, NULL, ZEND_ACC_PUBLIC)

     /* introspection */
     PHP_ME(Redis, getHost, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getPort, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getDBNum, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getTimeout, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getReadTimeout, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getPersistentID, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getAuth, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, isConnected, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, getMode, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, wait, NULL, ZEND_ACC_PUBLIC)
     PHP_ME(Redis, pubsub, NULL, ZEND_ACC_PUBLIC)

     /* aliases */
     PHP_MALIAS(Redis, open, connect, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, popen, pconnect, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, lLen, lSize, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, sGetMembers, sMembers, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, mget, getMultiple, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, expire, setTimeout, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zunionstore, zUnion, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zinterstore, zInter, NULL, ZEND_ACC_PUBLIC)

     PHP_MALIAS(Redis, zRemove, zDelete, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zRem, zDelete, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zRemoveRangeByScore, zDeleteRangeByScore, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zRemRangeByScore, zDeleteRangeByScore, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zRemRangeByRank, zDeleteRangeByRank, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zSize, zCard, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, substr, getRange, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, rename, renameKey, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, del, delete, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, keys, getKeys, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, lrem, lRemove, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, ltrim, listTrim, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, lindex, lGet, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, lrange, lGetRange, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, scard, sSize, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, srem, sRemove, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, sismember, sContains, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, zReverseRange, zRevRange, NULL, ZEND_ACC_PUBLIC)

     PHP_MALIAS(Redis, sendEcho, echo, NULL, ZEND_ACC_PUBLIC)

     PHP_MALIAS(Redis, evaluate, eval, NULL, ZEND_ACC_PUBLIC)
     PHP_MALIAS(Redis, evaluateSha, evalsha, NULL, ZEND_ACC_PUBLIC)
     {NULL, NULL, NULL}
};

zend_module_entry redis_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
     STANDARD_MODULE_HEADER,
#endif
     "redis",
     NULL,
     PHP_MINIT(redis),
     PHP_MSHUTDOWN(redis),
     PHP_RINIT(redis),
     PHP_RSHUTDOWN(redis),
     PHP_MINFO(redis),
#if ZEND_MODULE_API_NO >= 20010901
     PHP_REDIS_VERSION,
#endif
     STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_REDIS
ZEND_GET_MODULE(redis)
#endif

PHP_REDIS_API zend_class_entry *redis_get_exception_base(int root TSRMLS_DC)
{
#if HAVE_SPL
        if (!root) {
                if (!spl_ce_RuntimeException) {
                        zend_class_entry **pce;

                        if (zend_hash_find(CG(class_table), "runtimeexception",
                                           sizeof("RuntimeException"), 
                                           (void**)&pce) == SUCCESS) 
                        {
                                spl_ce_RuntimeException = *pce;
                                return *pce;
                        }
                } else {
                        return spl_ce_RuntimeException;
                }
        }
#endif
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
        return zend_exception_get_default();
#else
        return zend_exception_get_default(TSRMLS_C);
#endif
}

/* Send a static DISCARD in case we're in MULTI mode. */
static int send_discard_static(RedisSock *redis_sock TSRMLS_DC) {

	int result = FAILURE;
	char *cmd, *resp;
   	int resp_len, cmd_len;

   	/* format our discard command */
   	cmd_len = redis_cmd_format_static(&cmd, "DISCARD", "");

   	/* send our DISCARD command */
   	if (redis_sock_write(redis_sock, cmd, cmd_len TSRMLS_CC) >= 0 &&
   	   (resp = redis_sock_read(redis_sock,&resp_len TSRMLS_CC)) != NULL) 
    {

   		/* success if we get OK */
   		result = (resp_len == 3 && strncmp(resp,"+OK", 3)==0) ? SUCCESS:FAILURE;

   		/* free our response */
   		efree(resp);
   	}

   	/* free our command */
   	efree(cmd);

   	/* return success/failure */
   	return result;
}

/**
 * redis_destructor_redis_sock
 */
static void redis_destructor_redis_sock(zend_rsrc_list_entry * rsrc TSRMLS_DC)
{
    RedisSock *redis_sock = (RedisSock *) rsrc->ptr;
    redis_sock_disconnect(redis_sock TSRMLS_CC);
    redis_free_socket(redis_sock);
}
/**
 * redis_sock_get
 */
PHP_REDIS_API int redis_sock_get(zval *id, RedisSock **redis_sock TSRMLS_DC, int no_throw)
{

    zval **socket;
    int resource_type;

    if (Z_TYPE_P(id) != IS_OBJECT || zend_hash_find(Z_OBJPROP_P(id), "socket",
                                  sizeof("socket"), (void **) &socket) == FAILURE) {
    	/* Throw an exception unless we've been requested not to */
        if(!no_throw) {
        	zend_throw_exception(redis_exception_ce, "Redis server went away", 0 TSRMLS_CC);
        }
        return -1;
    }

    *redis_sock = (RedisSock *) zend_list_find(Z_LVAL_PP(socket), &resource_type);

    if (!*redis_sock || resource_type != le_redis_sock) {
		/* Throw an exception unless we've been requested not to */
    	if(!no_throw) {
    		zend_throw_exception(redis_exception_ce, "Redis server went away", 0 TSRMLS_CC);
    	}
		return -1;
    }
    if ((*redis_sock)->lazy_connect)
    {
        (*redis_sock)->lazy_connect = 0;
        if (redis_sock_server_open(*redis_sock, 1 TSRMLS_CC) < 0) {
            return -1;
        }
    }

    return Z_LVAL_PP(socket);
}

/**
 * redis_sock_get_direct
 * Returns our attached RedisSock pointer if we're connected
 */
PHP_REDIS_API RedisSock *redis_sock_get_connected(INTERNAL_FUNCTION_PARAMETERS) {
    zval *object;
    RedisSock *redis_sock;

    /* If we can't grab our object, or get a socket, or we're not connected, return NULL */
    if((zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, redis_ce) == FAILURE) ||
       (redis_sock_get(object, &redis_sock TSRMLS_CC, 1) < 0) || redis_sock->status != REDIS_SOCK_STATUS_CONNECTED)
    {
        return NULL;
    }

    /* Return our socket */
    return redis_sock;
}

/* Redis and RedisCluster objects share serialization/prefixing settings so 
 * this is a generic function to add class constants to either */
static void add_class_constants(zend_class_entry *ce, int only_atomic) {
    add_constant_long(ce, "REDIS_NOT_FOUND", REDIS_NOT_FOUND);
    add_constant_long(ce, "REDIS_STRING", REDIS_STRING);
    add_constant_long(ce, "REDIS_SET", REDIS_SET);
    add_constant_long(ce, "REDIS_LIST", REDIS_LIST);
    add_constant_long(ce, "REDIS_ZSET", REDIS_ZSET);
    add_constant_long(ce, "REDIS_HASH", REDIS_HASH);

    /* Cluster doesn't support pipelining at this time */
    if(!only_atomic) {
        add_constant_long(ce, "ATOMIC", ATOMIC);
        add_constant_long(ce, "MULTI", MULTI);
        add_constant_long(ce, "PIPELINE", PIPELINE);
    }

    /* options */
    add_constant_long(ce, "OPT_SERIALIZER", REDIS_OPT_SERIALIZER);
    add_constant_long(ce, "OPT_PREFIX", REDIS_OPT_PREFIX);
    add_constant_long(ce, "OPT_READ_TIMEOUT", REDIS_OPT_READ_TIMEOUT);

    /* serializer */
    add_constant_long(ce, "SERIALIZER_NONE", REDIS_SERIALIZER_NONE);
    add_constant_long(ce, "SERIALIZER_PHP", REDIS_SERIALIZER_PHP);

    /* scan options*/
    add_constant_long(ce, "OPT_SCAN", REDIS_OPT_SCAN);
    add_constant_long(ce, "SCAN_RETRY", REDIS_SCAN_RETRY);
    add_constant_long(ce, "SCAN_NORETRY", REDIS_SCAN_NORETRY);
#ifdef HAVE_REDIS_IGBINARY
    add_constant_long(ce, "SERIALIZER_IGBINARY", REDIS_SERIALIZER_IGBINARY);
#endif

    zend_declare_class_constant_stringl(ce, "AFTER", 5, "after", 5 TSRMLS_CC);
    zend_declare_class_constant_stringl(ce, "BEFORE", 6, "before", 6 TSRMLS_CC);
}

/**
 * PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(redis)
{
    zend_class_entry redis_class_entry;
    zend_class_entry redis_array_class_entry;
    zend_class_entry redis_cluster_class_entry;
    zend_class_entry redis_exception_class_entry;
    zend_class_entry redis_cluster_exception_class_entry;

	REGISTER_INI_ENTRIES();

	/* Redis class */
	INIT_CLASS_ENTRY(redis_class_entry, "Redis", redis_functions);
    redis_ce = zend_register_internal_class(&redis_class_entry TSRMLS_CC);

	/* RedisArray class */
	INIT_CLASS_ENTRY(redis_array_class_entry, "RedisArray", redis_array_functions);
    redis_array_ce = zend_register_internal_class(&redis_array_class_entry TSRMLS_CC);

    /* RedisCluster class */
    INIT_CLASS_ENTRY(redis_cluster_class_entry, "RedisCluster", redis_cluster_functions);
    redis_cluster_ce = zend_register_internal_class(&redis_cluster_class_entry TSRMLS_CC);
    redis_cluster_ce->create_object = create_cluster_context;

    le_redis_array = zend_register_list_destructors_ex(
        redis_destructor_redis_array,
        NULL,
        "Redis Array", module_number
    );

	/* RedisException class */
    INIT_CLASS_ENTRY(redis_exception_class_entry, "RedisException", NULL);
    redis_exception_ce = zend_register_internal_class_ex(
        &redis_exception_class_entry,
        redis_get_exception_base(0 TSRMLS_CC),
        NULL TSRMLS_CC
    );

    /* RedisClusterException class */
    INIT_CLASS_ENTRY(redis_cluster_exception_class_entry, 
        "RedisClusterException", NULL);
    redis_cluster_exception_ce = zend_register_internal_class_ex(
        &redis_cluster_exception_class_entry, 
        rediscluster_get_exception_base(0 TSRMLS_CC),
        NULL TSRMLS_CC
    );

    le_redis_sock = zend_register_list_destructors_ex(
        redis_destructor_redis_sock,
        NULL,
        redis_sock_name, module_number
    );

    /* Add class constants to Redis and RedisCluster objects */
    add_class_constants(redis_ce, 0);
    add_class_constants(redis_cluster_ce, 1);

    return SUCCESS;
}

/**
 * PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(redis)
{
    return SUCCESS;
}

/**
 * PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(redis)
{
    return SUCCESS;
}

/**
 * PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(redis)
{
    return SUCCESS;
}

/**
 * PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(redis)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Redis Support", "enabled");
    php_info_print_table_row(2, "Redis Version", PHP_REDIS_VERSION);
    php_info_print_table_end();
}

/* {{{ proto Redis Redis::__construct()
    Public constructor */
PHP_METHOD(Redis, __construct)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto Redis Redis::__destruct()
    Public Destructor
 */
PHP_METHOD(Redis,__destruct) {
	RedisSock *redis_sock;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	/* Grab our socket */
	if (redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 1) < 0) {
		RETURN_FALSE;
	}

	/* If we think we're in MULTI mode, send a discard */
	if(redis_sock->mode == MULTI) {
		/* Discard any multi commands, and free any callbacks that have been queued */
		send_discard_static(redis_sock TSRMLS_CC);
		free_reply_callbacks(getThis(), redis_sock);
	}
}

/* {{{ proto boolean Redis::connect(string host, int port [, double timeout [, long retry_interval]])
 */
PHP_METHOD(Redis, connect)
{
	if (redis_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0) == FAILURE) {
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto boolean Redis::pconnect(string host, int port [, double timeout])
 */
PHP_METHOD(Redis, pconnect)
{
	if (redis_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1) == FAILURE) {
		RETURN_FALSE;
	} else {
		/* reset multi/exec state if there is one. */
		RedisSock *redis_sock;
		if (redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 0) < 0) {
			RETURN_FALSE;
		}

		RETURN_TRUE;
	}
}
/* }}} */

PHP_REDIS_API int redis_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent) {
	zval *object;
	zval **socket;
	int host_len, id;
	char *host = NULL;
	long port = -1;
	long retry_interval = 0;

	char *persistent_id = NULL;
	int persistent_id_len = -1;

	double timeout = 0.0;
	RedisSock *redis_sock  = NULL;

#ifdef ZTS
	/* not sure how in threaded mode this works so disabled persistence at first */
    persistent = 0;
#endif

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|ldsl",
				&object, redis_ce, &host, &host_len, &port,
				&timeout, &persistent_id, &persistent_id_len,
				&retry_interval) == FAILURE) {
		return FAILURE;
	}

	if (timeout < 0L || timeout > INT_MAX) {
		zend_throw_exception(redis_exception_ce, "Invalid timeout", 0 TSRMLS_CC);
		return FAILURE;
	}

	if (retry_interval < 0L || retry_interval > INT_MAX) {
		zend_throw_exception(redis_exception_ce, "Invalid retry interval", 0 TSRMLS_CC);
		return FAILURE;
	}

	if(port == -1 && host_len && host[0] != '/') { /* not unix socket, set to default value */
		port = 6379;
	}

	/* if there is a redis sock already we have to remove it from the list */
	if (redis_sock_get(object, &redis_sock TSRMLS_CC, 1) > 0) {
		if (zend_hash_find(Z_OBJPROP_P(object), "socket",
					sizeof("socket"), (void **) &socket) == FAILURE)
		{
			/* maybe there is a socket but the id isn't known.. what to do? */
		} else {
			zend_list_delete(Z_LVAL_PP(socket)); /* the refcount should be decreased and the destructor called */
		}
	}

	redis_sock = redis_sock_create(host, host_len, port, timeout, persistent, persistent_id, retry_interval, 0);

	if (redis_sock_server_open(redis_sock, 1 TSRMLS_CC) < 0) {
		redis_free_socket(redis_sock);
		return FAILURE;
	}

#if PHP_VERSION_ID >= 50400
	id = zend_list_insert(redis_sock, le_redis_sock TSRMLS_CC);
#else
	id = zend_list_insert(redis_sock, le_redis_sock);
#endif
	add_property_resource(object, "socket", id);

	return SUCCESS;
}

/* {{{ proto long Redis::bitop(string op, string key, ...) */
PHP_METHOD(Redis, bitop)
{
    REDIS_PROCESS_CMD(bitop, redis_long_response);
}
/* }}} */

/* {{{ proto long Redis::bitcount(string key, [int start], [int end])
 */
PHP_METHOD(Redis, bitcount)
{
    REDIS_PROCESS_CMD(bitcount, redis_long_response);
}
/* }}} */

/* {{{ proto integer Redis::bitpos(string key, int bit, [int start], [int end]) */
PHP_METHOD(Redis, bitpos)
{
    REDIS_PROCESS_CMD(bitpos, redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::close()
 */
PHP_METHOD(Redis, close)
{
    zval *object;
    RedisSock *redis_sock = NULL;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O",
        &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    if (redis_sock_disconnect(redis_sock TSRMLS_CC)) {
        RETURN_TRUE;
    }

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto boolean Redis::set(string key, mixed value, long timeout | array options) */
PHP_METHOD(Redis, set) {
    REDIS_PROCESS_CMD(set, redis_boolean_response);
}

/* {{{ proto boolean Redis::setex(string key, long expire, string value)
 */
PHP_METHOD(Redis, setex)
{
    REDIS_PROCESS_KW_CMD("SETEX", redis_key_long_val_cmd, 
        redis_string_response);
}

/* {{{ proto boolean Redis::psetex(string key, long expire, string value)
 */
PHP_METHOD(Redis, psetex)
{
    REDIS_PROCESS_KW_CMD("PSETEX", redis_key_long_val_cmd, 
        redis_string_response);
}

/* {{{ proto boolean Redis::setnx(string key, string value)
 */
PHP_METHOD(Redis, setnx)
{
    REDIS_PROCESS_KW_CMD("SETNX", redis_kv_cmd, redis_1_response);
}

/* }}} */

/* {{{ proto string Redis::getSet(string key, string value)
 */
PHP_METHOD(Redis, getSet)
{
    REDIS_PROCESS_KW_CMD("GETSET", redis_kv_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string Redis::randomKey()
 */
PHP_METHOD(Redis, randomKey)
{
    REDIS_PROCESS_KW_CMD("RANDOMKEY", redis_empty_cmd, redis_ping_response);
}
/* }}} */

/* {{{ proto string Redis::echo(string msg)
 */
PHP_METHOD(Redis, echo)
{
    REDIS_PROCESS_KW_CMD("ECHO", redis_str_cmd, redis_string_response);

}
/* }}} */

/* {{{ proto string Redis::renameKey(string key_src, string key_dst)
 */
PHP_METHOD(Redis, renameKey)
{
    REDIS_PROCESS_KW_CMD("RENAME", redis_key_key_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto string Redis::renameNx(string key_src, string key_dst)
 */
PHP_METHOD(Redis, renameNx)
{
    REDIS_PROCESS_KW_CMD("RENAMENX", redis_key_key_cmd, redis_1_response);
}
/* }}} */

/* }}} */

/* {{{ proto string Redis::get(string key)
 */
PHP_METHOD(Redis, get)
{
    REDIS_PROCESS_KW_CMD("GET", redis_key_cmd, redis_string_response);
}
/* }}} */


/* {{{ proto string Redis::ping()
 */
PHP_METHOD(Redis, ping)
{
    REDIS_PROCESS_KW_CMD("PING", redis_empty_cmd, redis_ping_response);
}
/* }}} */

/* {{{ proto boolean Redis::incr(string key [,int value])
 */
PHP_METHOD(Redis, incr){
    REDIS_PROCESS_KW_CMD("INCR", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::incrBy(string key ,int value)
 */
PHP_METHOD(Redis, incrBy){
    REDIS_PROCESS_KW_CMD("INCRBY", redis_key_long_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto float Redis::incrByFloat(string key, float value)
 */
PHP_METHOD(Redis, incrByFloat) {
    REDIS_PROCESS_KW_CMD("INCRBYFLOAT", redis_key_dbl_cmd, 
        redis_bulk_double_response);
}
/* }}} */

/* {{{ proto boolean Redis::decr(string key) */
PHP_METHOD(Redis, decr)
{
    REDIS_PROCESS_KW_CMD("DECR", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::decrBy(string key ,int value)
 */
PHP_METHOD(Redis, decrBy){
    REDIS_PROCESS_KW_CMD("DECRBY", redis_key_long_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto array Redis::getMultiple(array keys)
 */
PHP_METHOD(Redis, getMultiple)
{
    zval *object, *z_args, **z_ele;
    HashTable *hash;
    HashPosition ptr;
    RedisSock *redis_sock;
    smart_str cmd = {0};
    int arg_count;

    /* Make sure we have proper arguments */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oa",
                                    &object, redis_ce, &z_args) == FAILURE) {
        RETURN_FALSE;
    }

    /* We'll need the socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    /* Grab our array */
    hash = Z_ARRVAL_P(z_args);

    /* We don't need to do anything if there aren't any keys */
    if((arg_count = zend_hash_num_elements(hash)) == 0) {
        RETURN_FALSE;
    }

    /* Build our command header */
    redis_cmd_init_sstr(&cmd, arg_count, "MGET", 4);

    /* Iterate through and grab our keys */
    for(zend_hash_internal_pointer_reset_ex(hash, &ptr);
        zend_hash_get_current_data_ex(hash, (void**)&z_ele, &ptr) == SUCCESS;
        zend_hash_move_forward_ex(hash, &ptr))
    {
        char *key;
        int key_len, key_free;
        zval *z_tmp = NULL;

        /* If the key isn't a string, turn it into one */
        if(Z_TYPE_PP(z_ele) == IS_STRING) {
            key = Z_STRVAL_PP(z_ele);
            key_len = Z_STRLEN_PP(z_ele);
        } else {
            MAKE_STD_ZVAL(z_tmp);
            *z_tmp = **z_ele;
            zval_copy_ctor(z_tmp);
            convert_to_string(z_tmp);

            key = Z_STRVAL_P(z_tmp);
            key_len = Z_STRLEN_P(z_tmp);
        }

        /* Apply key prefix if necissary */
        key_free = redis_key_prefix(redis_sock, &key, &key_len);

        /* Append this key to our command */
        redis_cmd_append_sstr(&cmd, key, key_len);

        /* Free our key if it was prefixed */
        if(key_free) efree(key);

        /* Free oour temporary ZVAL if we converted from a non-string */
        if(z_tmp) {
            zval_dtor(z_tmp);
            efree(z_tmp);
            z_tmp = NULL;
        }
    }

    /* Kick off our command */
    REDIS_PROCESS_REQUEST(redis_sock, cmd.c, cmd.len);
    IF_ATOMIC() {
        if(redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                           redis_sock, NULL, NULL) < 0) {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}

/* {{{ proto boolean Redis::exists(string key)
 */
PHP_METHOD(Redis, exists)
{
    REDIS_PROCESS_KW_CMD("EXISTS", redis_key_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto boolean Redis::delete(string key)
 */
PHP_METHOD(Redis, delete)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "DEL", sizeof("DEL") - 1,
					1, &redis_sock, 0, 1, 1))
		return;

    IF_ATOMIC() {
	  redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
	REDIS_PROCESS_RESPONSE(redis_long_response);

}
/* }}} */

PHP_REDIS_API void redis_set_watch(RedisSock *redis_sock)
{
    redis_sock->watching = 1;
}

PHP_REDIS_API void redis_watch_response(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock, zval *z_tab, void *ctx)
{
    redis_boolean_response_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, z_tab, ctx, redis_set_watch);
}

/* {{{ proto boolean Redis::watch(string key1, string key2...)
 */
PHP_METHOD(Redis, watch)
{
    RedisSock *redis_sock;

    generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "WATCH", sizeof("WATCH") - 1,
					1, &redis_sock, 0, 1, 1);
    redis_sock->watching = 1;
    IF_ATOMIC() {
        redis_watch_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_watch_response);

}
/* }}} */

PHP_REDIS_API void redis_clear_watch(RedisSock *redis_sock)
{
    redis_sock->watching = 0;
}

PHP_REDIS_API void redis_unwatch_response(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock, zval *z_tab, void *ctx)
{
    redis_boolean_response_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, z_tab, ctx, redis_clear_watch);
}

/* {{{ proto boolean Redis::unwatch()
 */
PHP_METHOD(Redis, unwatch)
{
    REDIS_PROCESS_KW_CMD("UNWATCH", redis_empty_cmd, redis_unwatch_response);
}
/* }}} */

/* {{{ proto array Redis::getKeys(string pattern)
 */
PHP_METHOD(Redis, getKeys)
{
    REDIS_PROCESS_KW_CMD("KEYS", redis_key_cmd, 
        redis_sock_read_multibulk_reply_raw);
}
/* }}} */

/* {{{ proto int Redis::type(string key)
 */
PHP_METHOD(Redis, type)
{
    REDIS_PROCESS_KW_CMD("TYPE", redis_key_cmd, redis_type_response);
}
/* }}} */

/* {{{ proto long Redis::append(string key, string val) */
PHP_METHOD(Redis, append)
{
    REDIS_PROCESS_KW_CMD("APPEND", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto string Redis::GetRange(string key, long start, long end) */
PHP_METHOD(Redis, getRange)
{
    REDIS_PROCESS_KW_CMD("GETRANGE", redis_key_long_long_cmd,
        redis_string_response);
}
/* }}} */

PHP_METHOD(Redis, setRange)
{
    REDIS_PROCESS_KW_CMD("SETRANGE", redis_key_long_str_cmd, 
        redis_long_response);
}

/* {{{ proto long Redis::getbit(string key, long idx) */
PHP_METHOD(Redis, getBit)
{
    REDIS_PROCESS_KW_CMD("GETBIT", redis_key_long_cmd, redis_long_response);
}
/* }}} */

PHP_METHOD(Redis, setBit)
{
    REDIS_PROCESS_CMD(setbit, redis_long_response);
}

/* {{{ proto long Redis::strlen(string key) */
PHP_METHOD(Redis, strlen)
{
    REDIS_PROCESS_KW_CMD("STRLEN", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::lPush(string key , string value)
 */
PHP_METHOD(Redis, lPush)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "LPUSH", sizeof("LPUSH") - 1,
                    2, &redis_sock, 0, 0, 1))
		return;

    IF_ATOMIC() {
        redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::rPush(string key , string value)
 */
PHP_METHOD(Redis, rPush)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "RPUSH", sizeof("RPUSH") - 1,
                    2, &redis_sock, 0, 0, 1))
		return;

    IF_ATOMIC() {
        redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_long_response);
}
/* }}} */

PHP_METHOD(Redis, lInsert)
{
    REDIS_PROCESS_CMD(linsert, redis_long_response);
}

/* {{{ proto long Redis::lPushx(string key, mixed value) */
PHP_METHOD(Redis, lPushx)
{	
    REDIS_PROCESS_KW_CMD("LPUSHX", redis_kv_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto long Redis::rPushx(string key, mixed value) */
PHP_METHOD(Redis, rPushx)
{
    REDIS_PROCESS_KW_CMD("RPUSHX", redis_kv_cmd, redis_string_response);
}

/* }}} */

/* {{{ proto string Redis::lPOP(string key)
 */
PHP_METHOD(Redis, lPop)
{
    REDIS_PROCESS_KW_CMD("LPOP", redis_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string Redis::rPOP(string key)
 */
PHP_METHOD(Redis, rPop)
{
    REDIS_PROCESS_KW_CMD("RPOP", redis_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string Redis::blPop(string key1, string key2, ..., int timeout)
 */
PHP_METHOD(Redis, blPop)
{

    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "BLPOP", sizeof("BLPOP") - 1,
					2, &redis_sock, 1, 1, 1))
		return;

    IF_ATOMIC() {
    	if (redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
											redis_sock, NULL, NULL) < 0) {
        	RETURN_FALSE;
	    }
    }
    REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto string Redis::brPop(string key1, string key2, ..., int timeout)
 */
PHP_METHOD(Redis, brPop)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "BRPOP", sizeof("BRPOP") - 1,
					2, &redis_sock, 1, 1, 1))
		return;

    IF_ATOMIC() {
    	if (redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
											redis_sock, NULL, NULL) < 0) {
        	RETURN_FALSE;
	    }
    }
    REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}
/* }}} */


/* {{{ proto int Redis::lSize(string key)
 */
PHP_METHOD(Redis, lSize)
{
    REDIS_PROCESS_KW_CMD("LLEN", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::lRemove(string list, string value, int count = 0)
 */
PHP_METHOD(Redis, lRemove)
{
    REDIS_PROCESS_CMD(lrem, redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::listTrim(string key , int start , int end)
 */
PHP_METHOD(Redis, listTrim)
{
    REDIS_PROCESS_KW_CMD("LTRIM", redis_key_long_long_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto string Redis::lGet(string key , int index)
 */
PHP_METHOD(Redis, lGet)
{
    REDIS_PROCESS_KW_CMD("LGET", redis_key_long_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto array Redis::lGetRange(string key, int start , int end)
 */
PHP_METHOD(Redis, lGetRange)
{
    REDIS_PROCESS_KW_CMD("LRANGE", redis_key_long_long_cmd, 
        redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto boolean Redis::sAdd(string key , mixed value)
 */
PHP_METHOD(Redis, sAdd)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SADD", sizeof("SADD") - 1,
					2, &redis_sock, 0, 0, 1))
		return;

	IF_ATOMIC() {
	  redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_long_response);
}
/* }}} */

/* {{{ proto int Redis::sSize(string key) */
PHP_METHOD(Redis, sSize)
{
    REDIS_PROCESS_KW_CMD("SMEMBERS", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean Redis::sRemove(string set, string value)
 */
PHP_METHOD(Redis, sRemove)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SREM", sizeof("SREM") - 1,
					2, &redis_sock, 0, 0, 1))
		return;

	IF_ATOMIC() {
	  redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_long_response);
}
/* }}} */
/* {{{ proto boolean Redis::sMove(string set_src, string set_dst, mixed value)
 */
PHP_METHOD(Redis, sMove)
{
    REDIS_PROCESS_CMD(smove, redis_1_response);
}
/* }}} */

/* }}} */
/* {{{ proto string Redis::sPop(string key)
 */
PHP_METHOD(Redis, sPop)
{
    REDIS_PROCESS_KW_CMD("SPOP", redis_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string Redis::sRandMember(string key [int count])
 */
PHP_METHOD(Redis, sRandMember)
{
    char *cmd;
    int cmd_len;
    short have_count;
    RedisSock *redis_sock;

    // Grab our socket, validate call
    if(redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 0)<0 ||
       redis_srandmember_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
                             &cmd, &cmd_len, NULL, NULL, &have_count)==FAILURE)
    {
        RETURN_FALSE;
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if(have_count) {
        IF_ATOMIC() {
            if(redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                               redis_sock, NULL, NULL)<0)
            {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
    } else {
        IF_ATOMIC() {
            redis_string_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
                NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_string_response);
    }
}
/* }}} */

/* {{{ proto boolean Redis::sContains(string set, string value)
 */
PHP_METHOD(Redis, sContains)
{
    REDIS_PROCESS_KW_CMD("SISMEMBER", redis_kv_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array Redis::sMembers(string set)
 */
PHP_METHOD(Redis, sMembers)
{
    REDIS_PROCESS_KW_CMD("SMEMBERS", redis_key_cmd,
        redis_sock_read_multibulk_reply);
}
/* }}} */

PHP_REDIS_API int generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAMETERS, 
                                            char *keyword, int keyword_len,
									        int min_argc, RedisSock **out_sock, 
                                            int has_timeout, int all_keys, 
                                            int can_serialize)
{
    zval **z_args, *z_array;
    char **keys, *cmd;
    int cmd_len, *keys_len, *keys_to_free;
    int i, j, argc = ZEND_NUM_ARGS(), real_argc = 0;
    int single_array = 0;
	int timeout = 0;
	int pos;
	int array_size;

    RedisSock *redis_sock;

    if(argc < min_argc) {
		zend_wrong_param_count(TSRMLS_C);
		ZVAL_BOOL(return_value, 0);
		return FAILURE;
    }

	/* get redis socket */
    if (redis_sock_get(getThis(), out_sock TSRMLS_CC, 0) < 0) {
		ZVAL_BOOL(return_value, 0);
		return FAILURE;
    }
    redis_sock = *out_sock;

    z_args = emalloc(argc * sizeof(zval*));
    if(zend_get_parameters_array(ht, argc, z_args) == FAILURE) {
        efree(z_args);
		ZVAL_BOOL(return_value, 0);
		return FAILURE;
    }

    /* case of a single array */
	if(has_timeout == 0) {
	    if(argc == 1 && Z_TYPE_P(z_args[0]) == IS_ARRAY) {
    	    single_array = 1;
        	z_array = z_args[0];
	        efree(z_args);
    	    z_args = NULL;

        	/* new count */
	        argc = zend_hash_num_elements(Z_ARRVAL_P(z_array));
    	}
	} else if(has_timeout == 1) {
		if(argc == 2 && Z_TYPE_P(z_args[0]) == IS_ARRAY && 
           Z_TYPE_P(z_args[1]) == IS_LONG) 
        {
    	    single_array = 1;
        	z_array = z_args[0];
			timeout = Z_LVAL_P(z_args[1]);
	        efree(z_args);
    	    z_args = NULL;
        	/* new count */
	        argc = zend_hash_num_elements(Z_ARRVAL_P(z_array));
    	}
	}

	/* prepare an array for the keys, one for their lengths, one to mark the 
     * keys to free. */
	array_size = argc;
	if(has_timeout)
		array_size++;

	keys = emalloc(array_size * sizeof(char*));
	keys_len = emalloc(array_size * sizeof(int));
	keys_to_free = emalloc(array_size * sizeof(int));
	memset(keys_to_free, 0, array_size * sizeof(int));


    /* Start computing the command length */
    cmd_len = 1 + integer_length(keyword_len) + 2 +keyword_len + 2;

    if(single_array) { /* loop over the array */
        HashTable *keytable = Z_ARRVAL_P(z_array);

        for(j = 0, zend_hash_internal_pointer_reset(keytable);
            zend_hash_has_more_elements(keytable) == SUCCESS;
            zend_hash_move_forward(keytable)) {

            char *key;
            unsigned int key_len;
            unsigned long idx;
            zval **z_value_pp;

            zend_hash_get_current_key_ex(keytable, &key, &key_len, &idx, 0, 
                NULL);
            if(zend_hash_get_current_data(keytable, (void**)&z_value_pp) 
                                          == FAILURE) 
            {
                /* this should never happen, according to the PHP people */
                continue;
            }


			if(!all_keys && j != 0) { /* not just operating on keys */

				if(can_serialize) {
					keys_to_free[j] = redis_serialize(redis_sock, *z_value_pp, 
                        &keys[j], &keys_len[j] TSRMLS_CC);
				} else {
					convert_to_string(*z_value_pp);
					keys[j] = Z_STRVAL_PP(z_value_pp);
					keys_len[j] = Z_STRLEN_PP(z_value_pp);
					keys_to_free[j] = 0;
				}

			} else {

				/* only accept strings */
				if(Z_TYPE_PP(z_value_pp) != IS_STRING) {
					convert_to_string(*z_value_pp);
				}

                /* get current value */
                keys[j] = Z_STRVAL_PP(z_value_pp);
                keys_len[j] = Z_STRLEN_PP(z_value_pp);

				keys_to_free[j] = redis_key_prefix(redis_sock, &keys[j], 
                    &keys_len[j]);
			}
            /* $ + size + NL + string + NL */
            cmd_len += 1 + integer_length(keys_len[j]) + 2 + keys_len[j] + 2; 
            j++;
            real_argc++;
        }
		if(has_timeout) {
			keys_len[j] = spprintf(&keys[j], 0, "%d", timeout);
			
            /* $ + size + NL + string + NL */
            cmd_len += 1 + integer_length(keys_len[j]) + 2 + keys_len[j] + 2;
			j++;
			real_argc++;
		}
    } else {
		if(has_timeout && Z_TYPE_P(z_args[argc - 1]) != IS_LONG) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, 
                "Syntax error on timeout");
		}

        for(i = 0, j = 0; i < argc; ++i) { /* store each key */
			if(!all_keys && j != 0) { /* not just operating on keys */
				if(can_serialize) {
					keys_to_free[j] = redis_serialize(redis_sock, z_args[i], 
                        &keys[j], &keys_len[j] TSRMLS_CC);
				} else {
					convert_to_string(z_args[i]);
					keys[j] = Z_STRVAL_P(z_args[i]);
					keys_len[j] = Z_STRLEN_P(z_args[i]);
					keys_to_free[j] = 0;
				}

			} else {

				if(Z_TYPE_P(z_args[i]) != IS_STRING) {
					convert_to_string(z_args[i]);
				}

       	        keys[j] = Z_STRVAL_P(z_args[i]);
           	    keys_len[j] = Z_STRLEN_P(z_args[i]);

           	    // If we have a timeout it should be the last argument, which 
                // we do not want to prefix
				if(!has_timeout || i < argc-1) {
					keys_to_free[j] = redis_key_prefix(redis_sock, &keys[j], 
                        &keys_len[j]);
				}
			}

            /* $ + size + NL + string + NL */
            cmd_len += 1 + integer_length(keys_len[j]) + 2 + keys_len[j] + 2;
            j++;
   	        real_argc++;
		}
    }

    cmd_len += 1 + integer_length(real_argc+1) + 2; /* *count NL  */
    cmd = emalloc(cmd_len+1);

    sprintf(cmd, "*%d" _NL "$%d" _NL "%s" _NL, 1+real_argc, keyword_len, 
        keyword);

    pos = 1 +integer_length(real_argc + 1) + 2
          + 1 + integer_length(keyword_len) + 2
          + keyword_len + 2;

    /* copy each key to its destination */
    for(i = 0; i < real_argc; ++i) {
        sprintf(cmd + pos, "$%d" _NL, keys_len[i]);     /* size */
        pos += 1 + integer_length(keys_len[i]) + 2;
        memcpy(cmd + pos, keys[i], keys_len[i]);
        pos += keys_len[i];
        memcpy(cmd + pos, _NL, 2);
        pos += 2;
    }

	/* cleanup prefixed keys. */
	for(i = 0; i < real_argc + (has_timeout?-1:0); ++i) {
		if(keys_to_free[i])
			STR_FREE(keys[i]);
	}

    /* Cleanup timeout string */
	if(single_array && has_timeout) {
		efree(keys[real_argc-1]);
	}

    efree(keys);
	efree(keys_len);
	efree(keys_to_free);

    if(z_args) efree(z_args);

	/* call REDIS_PROCESS_REQUEST and skip void returns */
	IF_MULTI_OR_ATOMIC() {
		if(redis_sock_write(redis_sock, cmd, cmd_len TSRMLS_CC) < 0) {
			efree(cmd);
			return FAILURE;
		}
		efree(cmd);
	}
	IF_PIPELINE() {
		PIPELINE_ENQUEUE_COMMAND(cmd, cmd_len);
		efree(cmd);
	}

    return SUCCESS;
}

/* {{{ proto array Redis::sInter(string key0, ... string keyN)
 */
PHP_METHOD(Redis, sInter) {

    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SINTER", sizeof("SINTER") - 1,
					0, &redis_sock, 0, 1, 1))
		return;

    IF_ATOMIC() {
    	if (redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
											redis_sock, NULL, NULL) < 0) {
        	RETURN_FALSE;
	    }
    }
    REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto array Redis::sInterStore(string destination, string key0, ... string keyN)
 */
PHP_METHOD(Redis, sInterStore) {

    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SINTERSTORE", sizeof("SINTERSTORE") - 1,
					1, &redis_sock, 0, 1, 1))
		return;

	IF_ATOMIC() {
		redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
    REDIS_PROCESS_RESPONSE(redis_long_response);


}
/* }}} */

/* {{{ proto array Redis::sUnion(string key0, ... string keyN)
 */
PHP_METHOD(Redis, sUnion) {

    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SUNION", sizeof("SUNION") - 1,
							  0, &redis_sock, 0, 1, 1))
		return;

	IF_ATOMIC() {
    	if (redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        	                                redis_sock, NULL, NULL) < 0) {
	        RETURN_FALSE;
    	}
	}
	REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}
/* }}} */
/* {{{ proto array Redis::sUnionStore(string destination, string key0, ... string keyN)
 */
PHP_METHOD(Redis, sUnionStore) {

    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SUNIONSTORE", sizeof("SUNIONSTORE") - 1,
					1, &redis_sock, 0, 1, 1))
		return;

	IF_ATOMIC() {
		redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_long_response);
}

/* }}} */

/* {{{ proto array Redis::sDiff(string key0, ... string keyN)
 */
PHP_METHOD(Redis, sDiff) {

    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SDIFF", sizeof("SDIFF") - 1,
					0, &redis_sock, 0, 1, 1))
		return;

	IF_ATOMIC() {
	    /* read multibulk reply */
    	if (redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
											redis_sock, NULL, NULL) < 0) {
        	RETURN_FALSE;
	    }
	}
	REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto array Redis::sDiffStore(string destination, string key0, ... string keyN)
 */
PHP_METHOD(Redis, sDiffStore) {

    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "SDIFFSTORE", sizeof("SDIFFSTORE") - 1,
					1, &redis_sock, 0, 1, 1))
		return;

	IF_ATOMIC() {
	  redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_long_response);
}
/* }}} */

PHP_METHOD(Redis, sort) {
    char *cmd; 
    int cmd_len, have_store;
    RedisSock *redis_sock;

    // Grab socket, handle command construction
    if(redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 0)<0 ||
       redis_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, &have_store,
                      &cmd, &cmd_len, NULL, NULL)==FAILURE)
    {
        RETURN_FALSE;
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if(have_store) {
        IF_ATOMIC() {
            redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
                NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_long_response);
    } else {
        IF_ATOMIC() {
            if(redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                               redis_sock, NULL, NULL)<0)
            {
                RETURN_FALSE;
            }
            REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
        }
    }
}

PHP_REDIS_API void generic_sort_cmd(INTERNAL_FUNCTION_PARAMETERS, char *sort, int use_alpha) {

    zval *object;
    RedisSock *redis_sock;
    char *key = NULL, *pattern = NULL, *get = NULL, *store = NULL, *cmd;
    int key_len, pattern_len = -1, get_len = -1, store_len = -1, cmd_len, key_free;
    long sort_start = -1, sort_count = -1;

    int cmd_elements;

    char *cmd_lines[30];
    int cmd_sizes[30];

	int sort_len;
    int i, pos;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|sslls",
                                     &object, redis_ce,
                                     &key, &key_len, &pattern, &pattern_len,
                                     &get, &get_len, &sort_start, &sort_count, &store, &store_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }
    if(key_len == 0) {
        RETURN_FALSE;
    }

    /* first line, sort. */
    cmd_lines[1] = estrdup("$4");
    cmd_sizes[1] = 2;
    cmd_lines[2] = estrdup("SORT");
    cmd_sizes[2] = 4;

    /* Prefix our key if we need to */
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    /* second line, key */
    cmd_sizes[3] = redis_cmd_format(&cmd_lines[3], "$%d", key_len);
    cmd_lines[4] = emalloc(key_len + 1);
    memcpy(cmd_lines[4], key, key_len);
    cmd_lines[4][key_len] = 0;
    cmd_sizes[4] = key_len;

    /* If we prefixed our key, free it */
    if(key_free) efree(key);

    cmd_elements = 5;
    if(pattern && pattern_len) {
            /* BY */
            cmd_lines[cmd_elements] = estrdup("$2");
            cmd_sizes[cmd_elements] = 2;
            cmd_elements++;
            cmd_lines[cmd_elements] = estrdup("BY");
            cmd_sizes[cmd_elements] = 2;
            cmd_elements++;

            /* pattern */
            cmd_sizes[cmd_elements] = redis_cmd_format(&cmd_lines[cmd_elements], "$%d", pattern_len);
            cmd_elements++;
            cmd_lines[cmd_elements] = emalloc(pattern_len + 1);
            memcpy(cmd_lines[cmd_elements], pattern, pattern_len);
            cmd_lines[cmd_elements][pattern_len] = 0;
            cmd_sizes[cmd_elements] = pattern_len;
            cmd_elements++;
    }
    if(sort_start >= 0 && sort_count >= 0) {
            /* LIMIT */
            cmd_lines[cmd_elements] = estrdup("$5");
            cmd_sizes[cmd_elements] = 2;
            cmd_elements++;
            cmd_lines[cmd_elements] = estrdup("LIMIT");
            cmd_sizes[cmd_elements] = 5;
            cmd_elements++;

            /* start */
            cmd_sizes[cmd_elements] = redis_cmd_format(&cmd_lines[cmd_elements], "$%d", integer_length(sort_start));
            cmd_elements++;
            cmd_sizes[cmd_elements] = spprintf(&cmd_lines[cmd_elements], 0, "%d", (int)sort_start);
            cmd_elements++;

            /* count */
            cmd_sizes[cmd_elements] = redis_cmd_format(&cmd_lines[cmd_elements], "$%d", integer_length(sort_count));
            cmd_elements++;
            cmd_sizes[cmd_elements] = spprintf(&cmd_lines[cmd_elements], 0, "%d", (int)sort_count);
            cmd_elements++;
    }
    if(get && get_len) {
            /* GET */
            cmd_lines[cmd_elements] = estrdup("$3");
            cmd_sizes[cmd_elements] = 2;
            cmd_elements++;
            cmd_lines[cmd_elements] = estrdup("GET");
            cmd_sizes[cmd_elements] = 3;
            cmd_elements++;

            /* pattern */
            cmd_sizes[cmd_elements] = redis_cmd_format(&cmd_lines[cmd_elements], "$%d", get_len);
            cmd_elements++;
            cmd_lines[cmd_elements] = emalloc(get_len + 1);
            memcpy(cmd_lines[cmd_elements], get, get_len);
            cmd_lines[cmd_elements][get_len] = 0;
            cmd_sizes[cmd_elements] = get_len;
            cmd_elements++;
    }

    /* add ASC or DESC */
    sort_len = strlen(sort);
    cmd_sizes[cmd_elements] = redis_cmd_format(&cmd_lines[cmd_elements], "$%d", sort_len);
    cmd_elements++;
    cmd_lines[cmd_elements] = emalloc(sort_len + 1);
    memcpy(cmd_lines[cmd_elements], sort, sort_len);
    cmd_lines[cmd_elements][sort_len] = 0;
    cmd_sizes[cmd_elements] = sort_len;
    cmd_elements++;

    if(use_alpha) {
            /* ALPHA */
            cmd_lines[cmd_elements] = estrdup("$5");
            cmd_sizes[cmd_elements] = 2;
            cmd_elements++;
            cmd_lines[cmd_elements] = estrdup("ALPHA");
            cmd_sizes[cmd_elements] = 5;
            cmd_elements++;
    }
    if(store && store_len) {
            /* STORE */
            cmd_lines[cmd_elements] = estrdup("$5");
            cmd_sizes[cmd_elements] = 2;
            cmd_elements++;
            cmd_lines[cmd_elements] = estrdup("STORE");
            cmd_sizes[cmd_elements] = 5;
            cmd_elements++;

            /* store key */
            cmd_sizes[cmd_elements] = redis_cmd_format(&cmd_lines[cmd_elements], "$%d", store_len);
            cmd_elements++;
            cmd_lines[cmd_elements] = emalloc(store_len + 1);
            memcpy(cmd_lines[cmd_elements], store, store_len);
            cmd_lines[cmd_elements][store_len] = 0;
            cmd_sizes[cmd_elements] = store_len;
            cmd_elements++;
    }

    /* first line has the star */
    cmd_sizes[0] = spprintf(&cmd_lines[0], 0, "*%d", (cmd_elements-1)/2);

    /* compute the command size */
    cmd_len = 0;
    for(i = 0; i < cmd_elements; ++i) {
            cmd_len += cmd_sizes[i] + sizeof(_NL) - 1; /* each line followeb by _NL */
    }

    /* copy all lines into the final command. */
    cmd = emalloc(1 + cmd_len);
    pos = 0;
    for(i = 0; i < cmd_elements; ++i) {
        memcpy(cmd + pos, cmd_lines[i], cmd_sizes[i]);
        pos += cmd_sizes[i];
        memcpy(cmd + pos, _NL, sizeof(_NL) - 1);
        pos += sizeof(_NL) - 1;

        /* free every line */
        efree(cmd_lines[i]);
    }

	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
    	if (redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
											redis_sock, NULL, NULL) < 0) {
        	RETURN_FALSE;
	    }
	}
	REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);

}

/* {{{ proto array Redis::sortAsc(string key, string pattern, string get, int start, int end, bool getList])
 */
PHP_METHOD(Redis, sortAsc)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ASC", 0);
}
/* }}} */

/* {{{ proto array Redis::sortAscAlpha(string key, string pattern, string get, int start, int end, bool getList])
 */
PHP_METHOD(Redis, sortAscAlpha)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ASC", 1);
}
/* }}} */

/* {{{ proto array Redis::sortDesc(string key, string pattern, string get, int start, int end, bool getList])
 */
PHP_METHOD(Redis, sortDesc)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "DESC", 0);
}
/* }}} */

/* {{{ proto array Redis::sortDescAlpha(string key, string pattern, string get, int start, int end, bool getList])
 */
PHP_METHOD(Redis, sortDescAlpha)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "DESC", 1);
}
/* }}} */

/* {{{ proto array Redis::setTimeout(string key, int timeout)
 */
PHP_METHOD(Redis, setTimeout) {
    REDIS_PROCESS_KW_CMD("EXPIRE", redis_key_long_cmd, redis_1_response);
}
/* }}} */

PHP_METHOD(Redis, pexpire) {
    REDIS_PROCESS_KW_CMD("PEXPIRE", redis_key_long_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array Redis::expireAt(string key, int timestamp)
 */
PHP_METHOD(Redis, expireAt) {
    REDIS_PROCESS_KW_CMD("EXPIREAT", redis_key_long_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array Redis::pexpireAt(string key, int timestamp)
 */
PHP_METHOD(Redis, pexpireAt) {
    REDIS_PROCESS_KW_CMD("PEXPIREAT", redis_key_long_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array Redis::lSet(string key, int index, string value)
 */
PHP_METHOD(Redis, lSet) {
    REDIS_PROCESS_KW_CMD("LSET", redis_key_long_val_cmd, 
        redis_boolean_response);
}
/* }}} */

/* {{{ proto string Redis::save()
 */
PHP_METHOD(Redis, save)
{
    REDIS_PROCESS_KW_CMD("SAVE", redis_empty_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto string Redis::bgSave()
 */
PHP_METHOD(Redis, bgSave)
{
    REDIS_PROCESS_KW_CMD("BGSAVE", redis_empty_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto integer Redis::lastSave()
 */
PHP_METHOD(Redis, lastSave)
{
    REDIS_PROCESS_KW_CMD("LASTSAVE", redis_empty_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto bool Redis::flushDB()
 */
PHP_METHOD(Redis, flushDB)
{
    REDIS_PROCESS_KW_CMD("FLUSHDB", redis_empty_cmd, redis_boolean_response);    
}
/* }}} */

/* {{{ proto bool Redis::flushAll()
 */
PHP_METHOD(Redis, flushAll)
{
    REDIS_PROCESS_KW_CMD("FLUSHALL", redis_empty_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto int Redis::dbSize()
 */
PHP_METHOD(Redis, dbSize)
{
    REDIS_PROCESS_KW_CMD("DBSIZE", redis_empty_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto bool Redis::auth(string passwd)
 */
PHP_METHOD(Redis, auth) {
    REDIS_PROCESS_CMD(auth, redis_boolean_response);
}
/* }}} */

/* {{{ proto long Redis::persist(string key)
 */
PHP_METHOD(Redis, persist) {
    REDIS_PROCESS_KW_CMD("PERSIST", redis_key_cmd, redis_1_response);
}
/* }}} */


/* {{{ proto long Redis::ttl(string key)
 */
PHP_METHOD(Redis, ttl) {
    REDIS_PROCESS_KW_CMD("TTL", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long Redis::pttl(string key)
 */
PHP_METHOD(Redis, pttl) {
    REDIS_PROCESS_KW_CMD("PTTL", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto array Redis::info()
 */
PHP_METHOD(Redis, info) {

    zval *object;
    RedisSock *redis_sock;
    char *cmd, *opt = NULL;
    int cmd_len, opt_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O|s",
                                     &object, redis_ce, &opt, &opt_len) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    /* Build a standalone INFO command or one with an option */
    if(opt != NULL) {
    	cmd_len = redis_cmd_format_static(&cmd, "INFO", "s", opt,
                                          opt_len);
    } else {
    	cmd_len = redis_cmd_format_static(&cmd, "INFO", "");
    }

	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
	  redis_info_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_info_response);

}
/* }}} */

/* {{{ proto bool Redis::select(long dbNumber)
 */
PHP_METHOD(Redis, select) {

    zval *object;
    RedisSock *redis_sock;

    char *cmd;
    int cmd_len;
    long dbNumber;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Ol",
                                     &object, redis_ce, &dbNumber) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    redis_sock->dbNumber = dbNumber;

    cmd_len = redis_cmd_format_static(&cmd, "SELECT", "d", dbNumber);

	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
		redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_boolean_response);
}
/* }}} */

/* {{{ proto bool Redis::move(string key, long dbindex)
 */
PHP_METHOD(Redis, move) {
    REDIS_PROCESS_KW_CMD("MOVE", redis_key_long_cmd, redis_1_response);
}
/* }}} */

PHP_REDIS_API void
generic_mset(INTERNAL_FUNCTION_PARAMETERS, char *kw, void (*fun)(INTERNAL_FUNCTION_PARAMETERS, RedisSock *, zval *, void *)) {

    zval *object;
    RedisSock *redis_sock;

    char *cmd = NULL, *p = NULL;
    int cmd_len = 0, argc = 0, kw_len = strlen(kw);
	int step = 0;	/* 0: compute size; 1: copy strings. */
    zval *z_array;

	HashTable *keytable;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oa",
                                     &object, redis_ce, &z_array) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    if(zend_hash_num_elements(Z_ARRVAL_P(z_array)) == 0) {
        RETURN_FALSE;
    }

	for(step = 0; step < 2; ++step) {
		if(step == 1) {
			cmd_len += 1 + integer_length(1 + 2 * argc) + 2;	/* star + arg count + NL */
			cmd_len += 1 + integer_length(kw_len) + 2;			/* dollar + strlen(kw) + NL */
			cmd_len += kw_len + 2;								/* kw + NL */

			p = cmd = emalloc(cmd_len + 1);	/* alloc */
			p += sprintf(cmd, "*%d" _NL "$%d" _NL "%s" _NL, 1 + 2 * argc, kw_len, kw); /* copy header */
		}

		keytable = Z_ARRVAL_P(z_array);
		for(zend_hash_internal_pointer_reset(keytable);
				zend_hash_has_more_elements(keytable) == SUCCESS;
				zend_hash_move_forward(keytable)) {

			char *key, *val;
			unsigned int key_len;
			int val_len;
			unsigned long idx;
			int type;
			zval **z_value_pp;
			int val_free, key_free;
			char buf[32];

			type = zend_hash_get_current_key_ex(keytable, &key, &key_len, &idx, 0, NULL);
			if(zend_hash_get_current_data(keytable, (void**)&z_value_pp) == FAILURE) {
				continue; 	/* this should never happen, according to the PHP people. */
			}

			/* If the key isn't a string, use the index value returned when grabbing the */
			/* key.  We typecast to long, because they could actually be negative. */
			if(type != HASH_KEY_IS_STRING) {
				/* Create string representation of our index */
				key_len = snprintf(buf, sizeof(buf), "%ld", (long)idx);
				key = (char*)buf;
			} else if(key_len > 0) {
				/* When not an integer key, the length will include the \0 */
				key_len--;
			}

			if(step == 0)
				argc++; /* found a valid arg */

			val_free = redis_serialize(redis_sock, *z_value_pp, &val, &val_len TSRMLS_CC);
			key_free = redis_key_prefix(redis_sock, &key, (int*)&key_len);

			if(step == 0) { /* counting */
				cmd_len += 1 + integer_length(key_len) + 2
						+ key_len + 2
						+ 1 + integer_length(val_len) + 2
						+ val_len + 2;
			} else {
				p += sprintf(p, "$%d" _NL, key_len);	/* key len */
				memcpy(p, key, key_len); p += key_len;	/* key */
				memcpy(p, _NL, 2); p += 2;

				p += sprintf(p, "$%d" _NL, val_len);	/* val len */
				memcpy(p, val, val_len); p += val_len;	/* val */
				memcpy(p, _NL, 2); p += 2;
			}

			if(val_free) STR_FREE(val);
			if(key_free) efree(key);
		}
	}

	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);

	IF_ATOMIC() {
		fun(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(fun);
}

/* {{{ proto bool Redis::mset(array (key => value, ...))
 */
PHP_METHOD(Redis, mset) {
	generic_mset(INTERNAL_FUNCTION_PARAM_PASSTHRU, "MSET", redis_boolean_response);
}
/* }}} */


/* {{{ proto bool Redis::msetnx(array (key => value, ...))
 */
PHP_METHOD(Redis, msetnx) {
	generic_mset(INTERNAL_FUNCTION_PARAM_PASSTHRU, "MSETNX", redis_1_response);
}
/* }}} */

/* {{{ proto string Redis::rpoplpush(string srckey, string dstkey)
 */
PHP_METHOD(Redis, rpoplpush)
{
    REDIS_PROCESS_KW_CMD("RPOPLPUSH", redis_key_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string Redis::brpoplpush(string src, string dst, int timeout) */
PHP_METHOD(Redis, brpoplpush) {
    REDIS_PROCESS_CMD(brpoplpush, redis_string_response);
}
/* }}} */

/* {{{ proto long Redis::zAdd(string key, int score, string value)
 */
PHP_METHOD(Redis, zAdd) {

    RedisSock *redis_sock;

    char *cmd;
    int cmd_len, key_len, val_len;
    double score;
    char *key, *val;
    int val_free, key_free = 0;
	char *dbl_str;
	int dbl_len;
    smart_str buf = {0};

	zval **z_args;
	int argc = ZEND_NUM_ARGS(), i;

	/* get redis socket */
    if (redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    z_args = emalloc(argc * sizeof(zval*));
    if(zend_get_parameters_array(ht, argc, z_args) == FAILURE) {
        efree(z_args);
		RETURN_FALSE;
    }

	/* need key, score, value, [score, value...] */
	if(argc > 1) {
		convert_to_string(z_args[0]); /* required string */
	}
	if(argc < 3 || Z_TYPE_P(z_args[0]) != IS_STRING || (argc-1) % 2 != 0) {
		efree(z_args);
		RETURN_FALSE;
	}

	/* possibly serialize key */
	key = Z_STRVAL_P(z_args[0]);
	key_len = Z_STRLEN_P(z_args[0]);
	key_free = redis_key_prefix(redis_sock, &key, &key_len);

	/* start building the command */
	smart_str_appendc(&buf, '*');
	smart_str_append_long(&buf, argc + 1); /* +1 for ZADD command */
	smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);

	/* add command name */
	smart_str_appendc(&buf, '$');
	smart_str_append_long(&buf, 4);
	smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);
	smart_str_appendl(&buf, "ZADD", 4);
	smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);

	/* add key */
	smart_str_appendc(&buf, '$');
	smart_str_append_long(&buf, key_len);
	smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);
	smart_str_appendl(&buf, key, key_len);
	smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);

	for(i = 1; i < argc; i +=2) {
		convert_to_double(z_args[i]); // convert score to double
		val_free = redis_serialize(redis_sock, z_args[i+1], &val, &val_len TSRMLS_CC); // possibly serialize value.

		/* add score */
		score = Z_DVAL_P(z_args[i]);
		REDIS_DOUBLE_TO_STRING(dbl_str, dbl_len, score)
		smart_str_appendc(&buf, '$');
		smart_str_append_long(&buf, dbl_len);
		smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);
		smart_str_appendl(&buf, dbl_str, dbl_len);
		smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);
		efree(dbl_str);

		/* add value */
		smart_str_appendc(&buf, '$');
		smart_str_append_long(&buf, val_len);
		smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);
		smart_str_appendl(&buf, val, val_len);
		smart_str_appendl(&buf, _NL, sizeof(_NL) - 1);

		if(val_free) STR_FREE(val);
	}

	/* end string */
	smart_str_0(&buf);
	cmd = buf.c;
	cmd_len = buf.len;
    if(key_free) efree(key);

	/* cleanup */
	efree(z_args);

	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
		redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_long_response);
}
/* }}} */

/* Handle ZRANGE and ZREVRANGE as they're the same except for keyword */
static void generic_zrange_cmd(INTERNAL_FUNCTION_PARAMETERS, char *kw, 
                               zrange_cb fun) 
{
    char *cmd;
    int cmd_len;
    RedisSock *redis_sock;
    int withscores=0;

    if(redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 0)<0) {
        RETURN_FALSE;
    }

    if(fun(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, kw, &cmd,
           &cmd_len, &withscores, NULL, NULL)==FAILURE)
    {
        RETURN_FALSE;
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if(withscores) {
        IF_ATOMIC() {
            redis_sock_read_multibulk_reply_zipped(
                INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply_zipped);
    } else {
        IF_ATOMIC() {
            if(redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                               redis_sock, NULL, NULL)<0)
            {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
    }
}

/* {{{ proto array Redis::zRange(string key,int start,int end,bool scores=0) */
PHP_METHOD(Redis, zRange)
{
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZRANGE",
        redis_zrange_cmd);
}

/* {{{ proto array Redis::zRevRange(string k, long s, long e, bool scores=0) */
PHP_METHOD(Redis, zRevRange) {
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZREVRANGE",
        redis_zrange_cmd);
}
/* }}} */

/* {{{ proto array Redis::zRangeByScore(string k,string s,string e,array opt) */
PHP_METHOD(Redis, zRangeByScore) {
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZRANGEBYSCORE",
        redis_zrangebyscore_cmd);
}
/* }}} */

/* {{{ proto array Redis::zRevRangeByScore(string key, string start, string end,
 *                                         array options) */
PHP_METHOD(Redis, zRevRangeByScore) {
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZREVRANGEBYSCORE",
        redis_zrangebyscore_cmd);
}
/* }}} */

/* {{{ proto long Redis::zDelete(string key, string member)
 */
PHP_METHOD(Redis, zDelete)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "ZREM", sizeof("ZREM") - 1,
					2, &redis_sock, 0, 0, 1))
		return;

	IF_ATOMIC() {
	  redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_long_response);
}
/* }}} */

/* {{{ proto long Redis::zDeleteRangeByScore(string k, string s, string e) */
PHP_METHOD(Redis, zDeleteRangeByScore)
{
    REDIS_PROCESS_KW_CMD("ZREMRANGEBYSCORE", redis_key_str_str_cmd,
        redis_long_response);
}
/* }}} */

/* {{{ proto long Redis::zDeleteRangeByRank(string key, long start, long end)
 */
PHP_METHOD(Redis, zDeleteRangeByRank)
{
    REDIS_PROCESS_KW_CMD("ZREMRANGEBYRANK", redis_key_long_long_cmd,
        redis_long_response);
}
/* }}} */

/* {{{ proto array Redis::zCount(string key, string start , string end) */
PHP_METHOD(Redis, zCount)
{
    REDIS_PROCESS_KW_CMD("ZCOUNT", redis_key_str_str_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto array Redis::zRangeByLex(string $key, string $min, string $max,
 *                                    [long $offset, long $count]) */
PHP_METHOD(Redis, zRangeByLex) {
    zval *object;
    RedisSock *redis_sock;
    char *cmd, *key, *min, *max;
    long offset, count;
    int argc, cmd_len, key_len;
    int key_free, min_len, max_len;

    /* We need either three or five arguments for this to be a valid call */
    argc = ZEND_NUM_ARGS();
    if (argc != 3 && argc != 5) {
        RETURN_FALSE;
    }

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(),
                                     "Osss|ll", &object, redis_ce, &key, &key_len,
                                     &min, &min_len, &max, &max_len, &offset,
                                     &count) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* We can do some simple validation for the user, as we know how min/max are
     * required to start */
    if (!IS_LEX_ARG(min,min_len) || !IS_LEX_ARG(max,max_len)) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    key_free = redis_key_prefix(redis_sock, &key, &key_len TSRMLS_CC);

    /* Construct our command depending on argc */
    if (argc == 3) {
        cmd_len = redis_cmd_format_static(&cmd, "ZRANGEBYLEX", "sss", key,
            key_len, min, min_len, max, max_len);
    } else {
        cmd_len = redis_cmd_format_static(&cmd, "ZRANGEBYLEX", "ssssll", key,
            key_len, min, min_len, max, max_len, "LIMIT", sizeof("LIMIT")-1,
            offset, count);
    }

    if(key_free) efree(key);

    /* Kick it off */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    IF_ATOMIC() {
        if (redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                            redis_sock, NULL, NULL) < 0) {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}

/* {{{ proto long Redis::zCard(string key)
 */
PHP_METHOD(Redis, zCard)
{
    REDIS_PROCESS_KW_CMD("ZCARD", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto double Redis::zScore(string key, mixed member)
 */
PHP_METHOD(Redis, zScore)
{
    REDIS_PROCESS_KW_CMD("ZSCORE", redis_kv_cmd, 
        redis_bulk_double_response);
}
/* }}} */

/* {{{ proto long Redis::zRank(string key, string member)
 */
PHP_METHOD(Redis, zRank) {
    REDIS_PROCESS_KW_CMD("ZRANK", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long Redis::zRevRank(string key, string member) */
PHP_METHOD(Redis, zRevRank) {
    REDIS_PROCESS_KW_CMD("ZREVRANK", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto double Redis::zIncrBy(string key, double value, mixed member)
 */
PHP_METHOD(Redis, zIncrBy)
{
    REDIS_PROCESS_CMD(zincrby, redis_bulk_double_response);
}
/* }}} */

PHP_REDIS_API void generic_z_command(INTERNAL_FUNCTION_PARAMETERS, char *command, int command_len) {
    zval *object, *z_keys, *z_weights = NULL, **z_data;
    HashTable *ht_keys, *ht_weights = NULL;
    RedisSock *redis_sock;
    smart_str cmd = {0};
    HashPosition ptr;
    char *store_key, *agg_op = NULL;
    int cmd_arg_count = 2, store_key_len, agg_op_len = 0, keys_count;
	int key_free;

    /* Grab our parameters */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osa|a!s",
                                    &object, redis_ce, &store_key, &store_key_len,
                                    &z_keys, &z_weights, &agg_op, &agg_op_len) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* We'll need our socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    /* Grab our keys argument as an array */
    ht_keys = Z_ARRVAL_P(z_keys);

    /* Nothing to do if there aren't any keys */
    if((keys_count = zend_hash_num_elements(ht_keys)) == 0) {
        RETURN_FALSE;
    } else {
        /* Increment our overall argument count */
        cmd_arg_count += keys_count;
    }

    /* Grab and validate our weights array */
    if(z_weights != NULL) {
        ht_weights = Z_ARRVAL_P(z_weights);

        /* This command is invalid if the weights array isn't the same size */
        /* as our keys array. */
        if(zend_hash_num_elements(ht_weights) != keys_count) {
            RETURN_FALSE;
        }

        /* Increment our overall argument count by the number of keys */
        /* plus one, for the "WEIGHTS" argument itself */
        cmd_arg_count += keys_count + 1;
    }

    /* AGGREGATE option */
    if(agg_op_len != 0) {
        /* Verify our aggregation option */
        if(strncasecmp(agg_op, "SUM", sizeof("SUM")) &&
           strncasecmp(agg_op, "MIN", sizeof("MIN")) &&
           strncasecmp(agg_op, "MAX", sizeof("MAX")))
        {
            RETURN_FALSE;
        }

        /* Two more arguments: "AGGREGATE" and agg_op */
        cmd_arg_count += 2;
    }

    /* Command header */
    redis_cmd_init_sstr(&cmd, cmd_arg_count, command, command_len);

    /* Prefix our key if necessary and add the output key */
    int key_free = redis_key_prefix(redis_sock, &store_key, &store_key_len);
    redis_cmd_append_sstr(&cmd, store_key, store_key_len);
    if(key_free) efree(store_key);

    /* Number of input keys argument */
    redis_cmd_append_sstr_int(&cmd, keys_count);

    /* Process input keys */
    for(zend_hash_internal_pointer_reset_ex(ht_keys, &ptr);
        zend_hash_get_current_data_ex(ht_keys, (void**)&z_data, &ptr)==SUCCESS;
        zend_hash_move_forward_ex(ht_keys, &ptr))
    {
        char *key;
        int key_free, key_len;
        zval *z_tmp = NULL;

        if(Z_TYPE_PP(z_data) == IS_STRING) {
            key = Z_STRVAL_PP(z_data);
            key_len = Z_STRLEN_PP(z_data);
        } else {
            MAKE_STD_ZVAL(z_tmp);
            *z_tmp = **z_data;
            convert_to_string(z_tmp);

            key = Z_STRVAL_P(z_tmp);
            key_len = Z_STRLEN_P(z_tmp);
        }

        /* Apply key prefix if necessary */
        key_free = redis_key_prefix(redis_sock, &key, &key_len);

        /* Append this input set */
        redis_cmd_append_sstr(&cmd, key, key_len);

        /* Free our key if it was prefixed */
        if(key_free) efree(key);

        /* Free our temporary z_val if it was converted */
        if(z_tmp) {
            zval_dtor(z_tmp);
            efree(z_tmp);
            z_tmp = NULL;
        }
    }

    /* Weights */
    if(ht_weights != NULL) {
        /* Append "WEIGHTS" argument */
        redis_cmd_append_sstr(&cmd, "WEIGHTS", sizeof("WEIGHTS") - 1);

        /* Process weights */
        for(zend_hash_internal_pointer_reset_ex(ht_weights, &ptr);
            zend_hash_get_current_data_ex(ht_weights, (void**)&z_data, &ptr)==SUCCESS;
            zend_hash_move_forward_ex(ht_weights, &ptr))
        {
            /* Ignore non numeric arguments, unless they're special Redis numbers */
            if (Z_TYPE_PP(z_data) != IS_LONG && Z_TYPE_PP(z_data) != IS_DOUBLE &&
                 strncasecmp(Z_STRVAL_PP(z_data), "inf", sizeof("inf")) != 0 &&
                 strncasecmp(Z_STRVAL_PP(z_data), "-inf", sizeof("-inf")) != 0 &&
                 strncasecmp(Z_STRVAL_PP(z_data), "+inf", sizeof("+inf")) != 0)
            {
                /* We should abort if we have an invalid weight, rather than pass */
                /* a different number of weights than the user is expecting */
                efree(cmd.c);
                RETURN_FALSE;
            }

            /* Append the weight based on the input type */
            switch(Z_TYPE_PP(z_data)) {
                case IS_LONG:
                    redis_cmd_append_sstr_long(&cmd, Z_LVAL_PP(z_data));
                    break;
                case IS_DOUBLE:
                    redis_cmd_append_sstr_dbl(&cmd, Z_DVAL_PP(z_data));
                    break;
                case IS_STRING:
                    redis_cmd_append_sstr(&cmd, Z_STRVAL_PP(z_data), Z_STRLEN_PP(z_data));
                    break;
            }
        }
    }

    /* Aggregation options, if we have them */
    if(agg_op_len != 0) {
        redis_cmd_append_sstr(&cmd, "AGGREGATE", sizeof("AGGREGATE") - 1);
        redis_cmd_append_sstr(&cmd, agg_op, agg_op_len);
    }

    /* Kick off our request */
    REDIS_PROCESS_REQUEST(redis_sock, cmd.c, cmd.len);
    IF_ATOMIC() {
        redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_long_response);
}

/* zInter */
PHP_METHOD(Redis, zInter) {
	generic_z_command(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZINTERSTORE", 11);
}

/* zUnion */
PHP_METHOD(Redis, zUnion) {
	generic_z_command(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZUNIONSTORE", 11);
}

/* hashes */

/* proto long Redis::hset(string key, string mem, string val) */
PHP_METHOD(Redis, hSet)
{
    REDIS_PROCESS_CMD(hset, redis_long_response);
}
/* }}} */

/* proto bool Redis::hSetNx(string key, string mem, string val) */
PHP_METHOD(Redis, hSetNx)
{
    REDIS_PROCESS_CMD(hsetnx, redis_1_response);
}
/* }}} */

/* proto string Redis::hget(string key, string mem) */
PHP_METHOD(Redis, hGet)
{
    REDIS_PROCESS_KW_CMD("HGET", redis_key_str_cmd, redis_string_response);
}
/* }}} */

/* hLen */
PHP_METHOD(Redis, hLen)
{
    REDIS_PROCESS_KW_CMD("HLEN", redis_key_cmd, redis_long_response);
}
/* }}} */

/* hDel */
PHP_METHOD(Redis, hDel)
{
    RedisSock *redis_sock;

    if(FAILURE == generic_multiple_args_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    "HDEL", sizeof("HDEL") - 1,
					2, &redis_sock, 0, 0, 0))
		return;

	IF_ATOMIC() {
	  redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_long_response);
}

/* {{{ proto bool Redis::hExists(string key, string mem) */
PHP_METHOD(Redis, hExists)
{
    REDIS_PROCESS_KW_CMD("HEXISTS", redis_key_str_cmd, redis_1_response);
}

/* {{{ proto array Redis::hkeys(string key) */
PHP_METHOD(Redis, hKeys)
{
    REDIS_PROCESS_KW_CMD("HKEYS", redis_key_cmd, 
        redis_sock_read_multibulk_reply_raw);
}
/* }}} */

/* {{{ proto array Redis::hvals(string key) */
PHP_METHOD(Redis, hVals)
{
    REDIS_PROCESS_KW_CMD("HVALS", redis_key_cmd, 
        redis_sock_read_multibulk_reply);
}

/* {{{ proto array Redis::hgetall(string key) */
PHP_METHOD(Redis, hGetAll) {
    REDIS_PROCESS_KW_CMD("HGETALL", redis_key_cmd, 
        redis_sock_read_multibulk_reply_zipped_strings);
}
/* }}} */

PHPAPI void array_zip_values_and_scores(RedisSock *redis_sock, zval *z_tab, int use_atof TSRMLS_DC) {

    zval *z_ret;
	HashTable *keytable;

	MAKE_STD_ZVAL(z_ret);
    array_init(z_ret);
    keytable = Z_ARRVAL_P(z_tab);

    for(zend_hash_internal_pointer_reset(keytable);
        zend_hash_has_more_elements(keytable) == SUCCESS;
        zend_hash_move_forward(keytable)) {

        char *tablekey, *hkey, *hval;
        unsigned int tablekey_len;
        int hkey_len;
        unsigned long idx;
        zval **z_key_pp, **z_value_pp;

        zend_hash_get_current_key_ex(keytable, &tablekey, &tablekey_len, &idx, 0, NULL);
        if(zend_hash_get_current_data(keytable, (void**)&z_key_pp) == FAILURE) {
            continue; 	/* this should never happen, according to the PHP people. */
        }

        /* get current value, a key */
        convert_to_string(*z_key_pp);
        hkey = Z_STRVAL_PP(z_key_pp);
        hkey_len = Z_STRLEN_PP(z_key_pp);

        /* move forward */
        zend_hash_move_forward(keytable);

        /* fetch again */
        zend_hash_get_current_key_ex(keytable, &tablekey, &tablekey_len, &idx, 0, NULL);
        if(zend_hash_get_current_data(keytable, (void**)&z_value_pp) == FAILURE) {
            continue; 	/* this should never happen, according to the PHP people. */
        }

        /* get current value, a hash value now. */
        hval = Z_STRVAL_PP(z_value_pp);

        if(use_atof) { /* zipping a score */
            add_assoc_double_ex(z_ret, hkey, 1+hkey_len, atof(hval));
        } else { /* add raw copy */
            zval *z = NULL;
            MAKE_STD_ZVAL(z);
            *z = **z_value_pp;
            zval_copy_ctor(z);
            add_assoc_zval_ex(z_ret, hkey, 1+hkey_len, z);
        }
    }
    /* replace */
    zval_dtor(z_tab);
    *z_tab = *z_ret;
    zval_copy_ctor(z_tab);
    zval_dtor(z_ret);

    efree(z_ret);
}

/* {{{ proto double Redis::hIncrByFloat(string k, string me, double v) */
PHP_METHOD(Redis, hIncrByFloat)
{
    REDIS_PROCESS_CMD(hincrbyfloat, redis_bulk_double_response);
}
/* }}} */

/* {{{ proto long Redis::hincrby(string key, string mem, long byval) */
PHP_METHOD(Redis, hIncrBy)
{
    REDIS_PROCESS_CMD(hincrby, redis_long_response);
}
/* }}} */

/* {{{ array Redis::hMget(string hash, array keys) */
PHP_METHOD(Redis, hMget) {
    REDIS_PROCESS_CMD(hmget, redis_sock_read_multibulk_reply_assoc);
}

/* {{{ proto bool Redis::hmset(string key, array keyvals) */
PHP_METHOD(Redis, hMset)
{
    REDIS_PROCESS_CMD(hmset, redis_boolean_response);
}
/* }}} */

/* flag : get, set {ATOMIC, MULTI, PIPELINE} */

PHP_METHOD(Redis, multi)
{

    RedisSock *redis_sock;
    char *cmd;
	int response_len, cmd_len;
	char * response;
	zval *object;
	long multi_value = MULTI;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O|l",
                                     &object, redis_ce, &multi_value) == FAILURE) {
        RETURN_FALSE;
    }

    /* if the flag is activated, send the command, the reply will be "QUEUED" or -ERR */

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

	if(multi_value == MULTI || multi_value == PIPELINE) {
		redis_sock->mode = multi_value;
	} else {
        RETURN_FALSE;
	}

    redis_sock->current = NULL;

	IF_MULTI() {
        cmd_len = redis_cmd_format_static(&cmd, "MULTI", "");

		if (redis_sock_write(redis_sock, cmd, cmd_len TSRMLS_CC) < 0) {
        	efree(cmd);
	        RETURN_FALSE;
    	}
	    efree(cmd);

    	if ((response = redis_sock_read(redis_sock, &response_len TSRMLS_CC)) == NULL) {
        	RETURN_FALSE;
    	}

        if(strncmp(response, "+OK", 3) == 0) {
            efree(response);
			RETURN_ZVAL(getThis(), 1, 0);
		}
        efree(response);
		RETURN_FALSE;
	}
	IF_PIPELINE() {
        free_reply_callbacks(getThis(), redis_sock);
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

/* discard */
PHP_METHOD(Redis, discard)
{
    RedisSock *redis_sock;
	zval *object;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O",
                                     &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

	redis_sock->mode = ATOMIC;
	redis_send_discard(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock);
}

PHP_REDIS_API int redis_sock_read_multibulk_pipeline_reply(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock)
{
    zval *z_tab;
    MAKE_STD_ZVAL(z_tab);
    array_init(z_tab);

    redis_sock_read_multibulk_multi_reply_loop(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    redis_sock, z_tab, 0);

    *return_value = *z_tab;
    efree(z_tab);

    /* free allocated function/request memory */
    free_reply_callbacks(getThis(), redis_sock);

    return 0;

}
/* redis_sock_read_multibulk_multi_reply */
PHP_REDIS_API int redis_sock_read_multibulk_multi_reply(INTERNAL_FUNCTION_PARAMETERS,
                                      RedisSock *redis_sock)
{

    char inbuf[1024];
	int numElems;
	zval *z_tab;

    redis_check_eof(redis_sock TSRMLS_CC);

    php_stream_gets(redis_sock->stream, inbuf, 1024);
    if(inbuf[0] != '*') {
        return -1;
    }

	/* number of responses */
    numElems = atoi(inbuf+1);

    if(numElems < 0) {
        return -1;
    }

    zval_dtor(return_value);

    MAKE_STD_ZVAL(z_tab);
    array_init(z_tab);

    redis_sock_read_multibulk_multi_reply_loop(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    redis_sock, z_tab, numElems);

    *return_value = *z_tab;
    efree(z_tab);

    return 0;
}

void
free_reply_callbacks(zval *z_this, RedisSock *redis_sock) {

	fold_item *fi;
    fold_item *head = redis_sock->head;
	request_item *ri;

	for(fi = head; fi; ) {
        fold_item *fi_next = fi->next;
        free(fi);
        fi = fi_next;
    }
    redis_sock->head = NULL;
    redis_sock->current = NULL;

    for(ri = redis_sock->pipeline_head; ri; ) {
        struct request_item *ri_next = ri->next;
        free(ri->request_str);
        free(ri);
        ri = ri_next;
    }
    redis_sock->pipeline_head = NULL;
    redis_sock->pipeline_current = NULL;
}

/* exec */
PHP_METHOD(Redis, exec)
{

    RedisSock *redis_sock;
    char *cmd;
	int cmd_len;
	zval *object;
    struct request_item *ri;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O",
                                     &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }
   	if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
       	RETURN_FALSE;
    }

	IF_MULTI() {

        cmd_len = redis_cmd_format_static(&cmd, "EXEC", "");

		if (redis_sock_write(redis_sock, cmd, cmd_len TSRMLS_CC) < 0) {
			efree(cmd);
			RETURN_FALSE;
		}
		efree(cmd);

	    if (redis_sock_read_multibulk_multi_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock) < 0) {
            zval_dtor(return_value);
            free_reply_callbacks(object, redis_sock);
            redis_sock->mode = ATOMIC;
            redis_sock->watching = 0;
			RETURN_FALSE;
	    }
        free_reply_callbacks(object, redis_sock);
		redis_sock->mode = ATOMIC;
        redis_sock->watching = 0;
	}

	IF_PIPELINE() {

		char *request = NULL;
		int total = 0;
		int offset = 0;

        /* compute the total request size */
		for(ri = redis_sock->pipeline_head; ri; ri = ri->next) {
            total += ri->request_size;
		}
        if(total) {
		    request = malloc(total);
        }

        /* concatenate individual elements one by one in the target buffer */
		for(ri = redis_sock->pipeline_head; ri; ri = ri->next) {
			memcpy(request + offset, ri->request_str, ri->request_size);
			offset += ri->request_size;
		}

		if(request != NULL) {
		    if (redis_sock_write(redis_sock, request, total TSRMLS_CC) < 0) {
    		    free(request);
                free_reply_callbacks(object, redis_sock);
                redis_sock->mode = ATOMIC;
        		RETURN_FALSE;
		    }
		   	free(request);
		} else {
                redis_sock->mode = ATOMIC;
                free_reply_callbacks(object, redis_sock);
                array_init(return_value); /* empty array when no command was run. */
                return;
        }

	    if (redis_sock_read_multibulk_pipeline_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock) < 0) {
			redis_sock->mode = ATOMIC;
            free_reply_callbacks(object, redis_sock);
			RETURN_FALSE;
	    }
		redis_sock->mode = ATOMIC;
        free_reply_callbacks(object, redis_sock);
	}
}

PHPAPI int redis_response_enqueued(RedisSock *redis_sock TSRMLS_DC) {
    char *resp;
    int resp_len, ret = 0;

    if ((resp = redis_sock_read(redis_sock, &resp_len TSRMLS_CC)) == NULL) {
        return 0;
    }

    if(strncmp(resp, "+QUEUED", 7) == 0) {
        ret = 1;
    }
    efree(resp);
    return ret;
}

PHPAPI void fold_this_item(INTERNAL_FUNCTION_PARAMETERS, fold_item *item, RedisSock *redis_sock, zval *z_tab) {
	item->fun(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, z_tab, item->ctx TSRMLS_CC);
}

PHP_REDIS_API int redis_sock_read_multibulk_multi_reply_loop(INTERNAL_FUNCTION_PARAMETERS,
							RedisSock *redis_sock, zval *z_tab, int numElems)
{

    fold_item *head = redis_sock->head;
    fold_item *current = redis_sock->current;
    for(current = head; current; current = current->next) {
		fold_this_item(INTERNAL_FUNCTION_PARAM_PASSTHRU, current, redis_sock, z_tab);
    }
    redis_sock->current = current;
    return 0;
}

PHP_METHOD(Redis, pipeline)
{
    RedisSock *redis_sock;
	zval *object;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O",
                                     &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }

    /* if the flag is activated, send the command, the reply will be "QUEUED" or -ERR */
    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }
	redis_sock->mode = PIPELINE;

	/*
		NB : we keep the function fold, to detect the last function.
		We need the response format of the n - 1 command. So, we can delete when n > 2, the { 1 .. n - 2} commands
	*/

    free_reply_callbacks(getThis(), redis_sock);

	RETURN_ZVAL(getThis(), 1, 0);
}

/* {{{ proto long Redis::publish(string channel, string msg) */
PHP_METHOD(Redis, publish)
{
    REDIS_PROCESS_KW_CMD("PUBLISH", redis_key_str_cmd, redis_long_response);
}
/* }}} */

PHP_REDIS_API void generic_subscribe_cmd(INTERNAL_FUNCTION_PARAMETERS, char *sub_cmd)
{
	zval *object, *array, **data;
    HashTable *arr_hash;
    HashPosition pointer;
    RedisSock *redis_sock;
    char *cmd = "", *old_cmd = NULL, *key;
    int cmd_len, array_count, key_len, key_free;
	zval *z_tab, **tmp;
	char *type_response;

	/* Function call information */
	zend_fcall_info z_callback;
	zend_fcall_info_cache z_callback_cache;

	zval *z_ret, **z_args[4];

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oaf",
									 &object, redis_ce, &array, &z_callback, &z_callback_cache) == FAILURE) {
		RETURN_FALSE;
	}

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    arr_hash    = Z_ARRVAL_P(array);
    array_count = zend_hash_num_elements(arr_hash);

    if (array_count == 0) {
        RETURN_FALSE;
    }
    for (zend_hash_internal_pointer_reset_ex(arr_hash, &pointer);
         zend_hash_get_current_data_ex(arr_hash, (void**) &data,
                                       &pointer) == SUCCESS;
         zend_hash_move_forward_ex(arr_hash, &pointer)) {

        if (Z_TYPE_PP(data) == IS_STRING) {
            char *old_cmd = NULL;
            if(*cmd) {
                old_cmd = cmd;
            }

            /* Grab our key and len */
            key = Z_STRVAL_PP(data);
            key_len = Z_STRLEN_PP(data);

            /* Prefix our key if neccisary */
            key_free = redis_key_prefix(redis_sock, &key, &key_len);

            cmd_len = spprintf(&cmd, 0, "%s %s", cmd, key);

            if(old_cmd) {
                efree(old_cmd);
            }

            /* Free our key if it was prefixed */
            if(key_free) {
            	efree(key);
            }
        }
    }

    old_cmd = cmd;
    cmd_len = spprintf(&cmd, 0, "%s %s\r\n", sub_cmd, cmd);
    efree(old_cmd);
    if (redis_sock_write(redis_sock, cmd, cmd_len TSRMLS_CC) < 0) {
        efree(cmd);
        RETURN_FALSE;
    }
    efree(cmd);

	/* read the status of the execution of the command `subscribe` */

    z_tab = redis_sock_read_multibulk_reply_zval(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock);
	if(z_tab == NULL) {
		RETURN_FALSE;
	}

	if (zend_hash_index_find(Z_ARRVAL_P(z_tab), 0, (void**)&tmp) == SUCCESS) {
		type_response = Z_STRVAL_PP(tmp);
		if(strcmp(type_response, sub_cmd) != 0) {
            efree(tmp);
			zval_dtor(z_tab);
            efree(z_tab);
			RETURN_FALSE;
		}
	} else {
		zval_dtor(z_tab);
        efree(z_tab);
		RETURN_FALSE;
	}
	zval_dtor(z_tab);
    efree(z_tab);

	/* Set a pointer to our return value and to our arguments. */
	z_callback.retval_ptr_ptr = &z_ret;
	z_callback.params = z_args;
	z_callback.no_separation = 0;

	/* Multibulk Response, format : {message type, originating channel, message payload} */
	while(1) {
		/* call the callback with this z_tab in argument */
	    int is_pmsg, tab_idx = 1;
		zval **type, **channel, **pattern, **data;
	    z_tab = redis_sock_read_multibulk_reply_zval(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock);
	    int is_pmsg, tab_idx = 1;

		if(z_tab == NULL || Z_TYPE_P(z_tab) != IS_ARRAY) {
			/*ERROR */
			break;
		}

		if (zend_hash_index_find(Z_ARRVAL_P(z_tab), 0, (void**)&type) == FAILURE || Z_TYPE_PP(type) != IS_STRING) {
			break;
		}

		/* Make sure we have a message or pmessage */
		if(!strncmp(Z_STRVAL_PP(type), "message", 7) || !strncmp(Z_STRVAL_PP(type), "pmessage", 8)) {
			/* Is this a pmessage */
			is_pmsg = *Z_STRVAL_PP(type) == 'p';
		} else {
			continue;  /* It's not a message or pmessage */
		}

		/* If this is a pmessage, we'll want to extract the pattern first */
		if(is_pmsg) {
			/* Extract pattern */
			if(zend_hash_index_find(Z_ARRVAL_P(z_tab), tab_idx++, (void**)&pattern) == FAILURE) {
				break;
			}
		}

		/* Extract channel and data */
		if (zend_hash_index_find(Z_ARRVAL_P(z_tab), tab_idx++, (void**)&channel) == FAILURE) {
			break;
		}
		if (zend_hash_index_find(Z_ARRVAL_P(z_tab), tab_idx++, (void**)&data) == FAILURE) {
			break;
		}

		/* Always pass the Redis object through */
		z_args[0] = &getThis();

		/* Set up our callback args depending on the message type */
		if(is_pmsg) {
			z_args[1] = pattern;
			z_args[2] = channel;
			z_args[3] = data;
		} else {
			z_args[1] = channel;
			z_args[2] = data;
		}

		/* Set our argument information */
		z_callback.param_count = tab_idx;

		/* Break if we can't call the function */
		if(zend_call_function(&z_callback, &z_callback_cache TSRMLS_CC) != SUCCESS) {
			break;
		}

        /* Free reply from Redis */
        zval_dtor(z_tab);
        efree(z_tab);        

        /* Check for a non-null return value.  If we have one, return it from
         * the subscribe function itself.  Otherwise continue our loop. */
        if (z_ret) {
            if (Z_TYPE_P(z_ret) != IS_NULL) {
                RETVAL_ZVAL(z_ret, 0, 1);
                break;
            }
            zval_ptr_dtor(&z_ret);
        }
	}
}

/* {{{ proto void Redis::psubscribe(Array(pattern1, pattern2, ... patternN))
 */
PHP_METHOD(Redis, psubscribe)
{
	generic_subscribe_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "psubscribe");
}

/* {{{ proto void Redis::subscribe(Array(channel1, channel2, ... channelN))
 */
PHP_METHOD(Redis, subscribe) {
	generic_subscribe_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "subscribe");
}

/**
 *	[p]unsubscribe channel_0 channel_1 ... channel_n
 *  [p]unsubscribe(array(channel_0, channel_1, ..., channel_n))
 * response format :
 * array(
 * 	channel_0 => TRUE|FALSE,
 *	channel_1 => TRUE|FALSE,
 *	...
 *	channel_n => TRUE|FALSE
 * );
 **/

PHP_REDIS_API void generic_unsubscribe_cmd(INTERNAL_FUNCTION_PARAMETERS, char *unsub_cmd)
{
    zval *object, *array, **data;
    HashTable *arr_hash;
    HashPosition pointer;
    RedisSock *redis_sock;
    char *cmd = "", *old_cmd = NULL;
    int cmd_len, array_count;

	int i;
	zval *z_tab, **z_channel;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oa",
									 &object, redis_ce, &array) == FAILURE) {
		RETURN_FALSE;
	}
    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    arr_hash    = Z_ARRVAL_P(array);
    array_count = zend_hash_num_elements(arr_hash);

    if (array_count == 0) {
        RETURN_FALSE;
    }

    for (zend_hash_internal_pointer_reset_ex(arr_hash, &pointer);
         zend_hash_get_current_data_ex(arr_hash, (void**) &data,
                                       &pointer) == SUCCESS;
         zend_hash_move_forward_ex(arr_hash, &pointer)) {

        if (Z_TYPE_PP(data) == IS_STRING) {
            char *old_cmd = NULL;
            if(*cmd) {
                old_cmd = cmd;
            }
            cmd_len = spprintf(&cmd, 0, "%s %s", cmd, Z_STRVAL_PP(data));
            if(old_cmd) {
                efree(old_cmd);
            }
        }
    }

    old_cmd = cmd;
    cmd_len = spprintf(&cmd, 0, "%s %s\r\n", unsub_cmd, cmd);
    efree(old_cmd);

    if (redis_sock_write(redis_sock, cmd, cmd_len TSRMLS_CC) < 0) {
        efree(cmd);
        RETURN_FALSE;
    }
    efree(cmd);

	i = 1;
	array_init(return_value);

	while( i <= array_count) {
	    z_tab = redis_sock_read_multibulk_reply_zval(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock);

		if(Z_TYPE_P(z_tab) == IS_ARRAY) {
			if (zend_hash_index_find(Z_ARRVAL_P(z_tab), 1, (void**)&z_channel) == FAILURE) {
				RETURN_FALSE;
			}
			add_assoc_bool(return_value, Z_STRVAL_PP(z_channel), 1);
		} else {
			/*error */
			efree(z_tab);
			RETURN_FALSE;
		}
		efree(z_tab);
		i ++;
	}
}

PHP_METHOD(Redis, unsubscribe)
{
	generic_unsubscribe_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "UNSUBSCRIBE");
}

PHP_METHOD(Redis, punsubscribe)
{
	generic_unsubscribe_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "PUNSUBSCRIBE");
}

/* {{{ proto string Redis::bgrewriteaof()
 */
PHP_METHOD(Redis, bgrewriteaof)
{
    REDIS_PROCESS_KW_CMD("BGREWRITEAOF", redis_empty_cmd, 
        redis_boolean_response);
}
/* }}} */

/* {{{ proto string Redis::slaveof([host, port])
 */
PHP_METHOD(Redis, slaveof)
{
    zval *object;
    RedisSock *redis_sock;
    char *cmd = "", *host = NULL;
    int cmd_len, host_len;
    long port = 6379;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O|sl",
									 &object, redis_ce, &host, &host_len, &port) == FAILURE) {
		RETURN_FALSE;
	}
    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    if(host && host_len) {
        cmd_len = redis_cmd_format_static(&cmd, "SLAVEOF", "sd", host,
                                          host_len, (int)port);
    } else {
        cmd_len = redis_cmd_format_static(&cmd, "SLAVEOF", "ss", "NO",
                                          2, "ONE", 3);
    }

	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    IF_ATOMIC() {
	  redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_boolean_response);
}
/* }}} */

/* {{{ proto string Redis::object(key)
 */
PHP_METHOD(Redis, object)
{
    zval *object;
    RedisSock *redis_sock;
    char *cmd = "", *info = NULL, *key = NULL;
    int cmd_len, info_len, key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss",
									 &object, redis_ce, &info, &info_len, &key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}
    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    cmd_len = redis_cmd_format_static(&cmd, "OBJECT", "ss", info,
                                      info_len, key, key_len);
	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);

	if(info_len == 8 && (strncasecmp(info, "refcount", 8) == 0 || strncasecmp(info, "idletime", 8) == 0)) {
		IF_ATOMIC() {
		  redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
		}
		REDIS_PROCESS_RESPONSE(redis_long_response);
	} else if(info_len == 8 && strncasecmp(info, "encoding", 8) == 0) {
		IF_ATOMIC() {
		  redis_string_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
		}
		REDIS_PROCESS_RESPONSE(redis_string_response);
	} else { /* fail */
		IF_ATOMIC() {
		  redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
		}
		REDIS_PROCESS_RESPONSE(redis_boolean_response);
	}
}
/* }}} */

/* {{{ proto string Redis::getOption($option)
 */
PHP_METHOD(Redis, getOption)  {
    RedisSock *redis_sock;
    zval *object;
    long option;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Ol",
									 &object, redis_ce, &option) == FAILURE) {
		RETURN_FALSE;
	}

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    switch(option) {
        case REDIS_OPT_SERIALIZER:
            RETURN_LONG(redis_sock->serializer);
        case REDIS_OPT_PREFIX:
            if(redis_sock->prefix) {
                RETURN_STRINGL(redis_sock->prefix, redis_sock->prefix_len, 1);
            }
            RETURN_NULL();
        case REDIS_OPT_READ_TIMEOUT:
            RETURN_DOUBLE(redis_sock->read_timeout);
        case REDIS_OPT_SCAN:
            RETURN_LONG(redis_sock->scan);
        default:
            RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto string Redis::setOption(string $option, mixed $value)
 */
PHP_METHOD(Redis, setOption) {
    RedisSock *redis_sock;
    zval *object;
    long option, val_long;
	char *val_str;
	int val_len;
    struct timeval read_tv;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Ols",
									 &object, redis_ce, &option, &val_str, &val_len) == FAILURE) {
		RETURN_FALSE;
	}

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    switch(option) {
        case REDIS_OPT_SERIALIZER:
            val_long = atol(val_str);
            if(val_long == REDIS_SERIALIZER_NONE
#ifdef HAVE_REDIS_IGBINARY
                    || val_long == REDIS_SERIALIZER_IGBINARY
#endif
                    || val_long == REDIS_SERIALIZER_PHP) {
                        redis_sock->serializer = val_long;
                        RETURN_TRUE;
                    } else {
                        RETURN_FALSE;
                    }
                    break;
			case REDIS_OPT_PREFIX:
			    if(redis_sock->prefix) {
			        efree(redis_sock->prefix);
			    }
			    if(val_len == 0) {
			        redis_sock->prefix = NULL;
			        redis_sock->prefix_len = 0;
			    } else {
			        redis_sock->prefix_len = val_len;
			        redis_sock->prefix = ecalloc(1+val_len, 1);
			        memcpy(redis_sock->prefix, val_str, val_len);
			    }
			    RETURN_TRUE;
            case REDIS_OPT_READ_TIMEOUT:
                redis_sock->read_timeout = atof(val_str);
                if(redis_sock->stream) {
                    read_tv.tv_sec  = (time_t)redis_sock->read_timeout;
                    read_tv.tv_usec = (int)((redis_sock->read_timeout - read_tv.tv_sec) * 1000000);
                    php_stream_set_option(redis_sock->stream, PHP_STREAM_OPTION_READ_TIMEOUT,0, &read_tv);
                }
                RETURN_TRUE;
            case REDIS_OPT_SCAN:
                val_long = atol(val_str);
                if(val_long == REDIS_SCAN_NORETRY || val_long == REDIS_SCAN_RETRY) {
                    redis_sock->scan = val_long;
                    RETURN_TRUE;
                }
                RETURN_FALSE;
                break;
            default:
                RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto boolean Redis::config(string op, string key [, mixed value])
 */
PHP_METHOD(Redis, config)
{
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL, *val = NULL, *cmd, *op = NULL;
    int key_len, val_len, cmd_len, op_len;
	enum {CFG_GET, CFG_SET} mode;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss|s",
                                     &object, redis_ce, &op, &op_len, &key, &key_len,
                                     &val, &val_len) == FAILURE) {
        RETURN_FALSE;
    }

	/* op must be GET or SET */
	if(strncasecmp(op, "GET", 3) == 0) {
		mode = CFG_GET;
	} else if(strncasecmp(op, "SET", 3) == 0) {
		mode = CFG_SET;
	} else {
		RETURN_FALSE;
	}

    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    if (mode == CFG_GET && val == NULL) {
        cmd_len = redis_cmd_format_static(&cmd, "CONFIG", "ss", op,
                                          op_len, key, key_len);

		REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len)
		IF_ATOMIC() {
			redis_mbulk_reply_zipped_raw(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
		}
		REDIS_PROCESS_RESPONSE(redis_mbulk_reply_zipped_raw);

    } else if(mode == CFG_SET && val != NULL) {
        cmd_len = redis_cmd_format_static(&cmd, "CONFIG", "sss", op,
                                          op_len, key, key_len, val, val_len);

		REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len)
		IF_ATOMIC() {
			redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
		}
		REDIS_PROCESS_RESPONSE(redis_boolean_response);
    } else {
		RETURN_FALSE;
	}
}
/* }}} */


/* {{{ proto boolean Redis::slowlog(string arg, [int option])
 */
PHP_METHOD(Redis, slowlog) {
    zval *object;
    RedisSock *redis_sock;
    char *arg, *cmd;
    int arg_len, cmd_len;
    long option;
    enum {SLOWLOG_GET, SLOWLOG_LEN, SLOWLOG_RESET} mode;

    /* Make sure we can get parameters */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|l",
                                    &object, redis_ce, &arg, &arg_len, &option) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* Figure out what kind of slowlog command we're executing */
    if(!strncasecmp(arg, "GET", 3)) {
        mode = SLOWLOG_GET;
    } else if(!strncasecmp(arg, "LEN", 3)) {
        mode = SLOWLOG_LEN;
    } else if(!strncasecmp(arg, "RESET", 5)) {
        mode = SLOWLOG_RESET;
    } else {
        /* This command is not valid */
        RETURN_FALSE;
    }

    /* Make sure we can grab our redis socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    /* Create our command.  For everything except SLOWLOG GET (with an arg) it's just two parts */
    if(mode == SLOWLOG_GET && ZEND_NUM_ARGS() == 2) {
        cmd_len = redis_cmd_format_static(&cmd, "SLOWLOG", "sl", arg,
                                          arg_len, option);
    } else {
        cmd_len = redis_cmd_format_static(&cmd, "SLOWLOG", "s", arg,
                                          arg_len);
    }

    /* Kick off our command */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    IF_ATOMIC() {
        if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL) < 0) {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

/* {{{ proto Redis::rawCommand(string cmd, arg, arg, arg, ...) }}} */
PHP_METHOD(Redis, rawCommand) {
    zval **z_args;
    RedisSock *redis_sock;
    int argc = ZEND_NUM_ARGS(), i;
    smart_str cmd = {0};

    /* We need at least one argument */
    z_args = emalloc(argc * sizeof(zval*));
    if (argc < 1 || zend_get_parameters_array(ht, argc, z_args) == FAILURE) {
        efree(z_args);
        RETURN_FALSE;
    }

    if (redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 0) < 0) {
        efree(z_args);
        RETURN_FALSE;
    }

    /* Initialize the command we'll send */
    convert_to_string(z_args[0]);
    redis_cmd_init_sstr(&cmd,argc-1,Z_STRVAL_P(z_args[0]),Z_STRLEN_P(z_args[0]));

    /* Iterate over the remainder of our arguments, appending */
    for (i = 1; i < argc; i++) {
        convert_to_string(z_args[i]);
        redis_cmd_append_sstr(&cmd, Z_STRVAL_P(z_args[i]), Z_STRLEN_P(z_args[i])); 
    }

    efree(z_args);

    /* Kick off our request and read response or enqueue handler */
    REDIS_PROCESS_REQUEST(redis_sock, cmd.c, cmd.len);
    IF_ATOMIC() {
        if (redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, 
                                     redis_sock, NULL) < 0)
        {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

/* {{{ proto Redis::wait(int num_slaves, int ms) }}}
 */
PHP_METHOD(Redis, wait) {
    zval *object;
    RedisSock *redis_sock;
    long num_slaves, timeout;
    char *cmd;
    int cmd_len;

    /* Make sure arguments are valid */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oll",
                                    &object, redis_ce, &num_slaves, &timeout)
                                    ==FAILURE)
    {
        RETURN_FALSE;
    }

    /* Don't even send this to Redis if our args are negative */
    if(num_slaves < 0 || timeout < 0) {
        RETURN_FALSE;
    }

    /* Grab our socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0)<0) {
        RETURN_FALSE;
    }

    // Construct the command
    cmd_len = redis_cmd_format_static(&cmd, "WAIT", "ll", num_slaves,
                                      timeout);

    /* Kick it off */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    IF_ATOMIC() {
        redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_long_response);
}

/*
 * Construct a PUBSUB command
 */
PHP_REDIS_API int
redis_build_pubsub_cmd(RedisSock *redis_sock, char **ret, PUBSUB_TYPE type,
                       zval *arg TSRMLS_DC)
{
    HashTable *ht_chan;
    HashPosition ptr;
    zval **z_ele;
    char *key;
    int cmd_len, key_len, key_free;
    smart_str cmd = {0};

    if(type == PUBSUB_CHANNELS) {
        if(arg) {
            /* Get string argument and length. */
            key = Z_STRVAL_P(arg);
            key_len = Z_STRLEN_P(arg);

            /* Prefix if necissary */
            key_free = redis_key_prefix(redis_sock, &key, &key_len);

            // With a pattern
            cmd_len = redis_cmd_format_static(ret, "PUBSUB", "ss",
                                              "CHANNELS", sizeof("CHANNELS")-1,
                                              key, key_len);

            /* Free the channel name if we prefixed it */
            if(key_free) efree(key);

            /* Return command length */
            return cmd_len;
        } else {
            // No pattern
            return redis_cmd_format_static(ret, "PUBSUB", "s",
                                           "CHANNELS", sizeof("CHANNELS")-1);
        }
    } else if(type == PUBSUB_NUMSUB) {
        ht_chan = Z_ARRVAL_P(arg);

        /* Add PUBSUB and NUMSUB bits */
        redis_cmd_init_sstr(&cmd, zend_hash_num_elements(ht_chan)+1, "PUBSUB", sizeof("PUBSUB")-1);
        redis_cmd_append_sstr(&cmd, "NUMSUB", sizeof("NUMSUB")-1);

        /* Iterate our elements */
        for(zend_hash_internal_pointer_reset_ex(ht_chan, &ptr);
            zend_hash_get_current_data_ex(ht_chan, (void**)&z_ele, &ptr)==SUCCESS;
            zend_hash_move_forward_ex(ht_chan, &ptr))
        {
            char *key;
            int key_len, key_free;
            zval *z_tmp = NULL;

            if(Z_TYPE_PP(z_ele) == IS_STRING) {
                key = Z_STRVAL_PP(z_ele);
                key_len = Z_STRLEN_PP(z_ele);
            } else {
                MAKE_STD_ZVAL(z_tmp);
                *z_tmp = **z_ele;
                zval_copy_ctor(z_tmp);
                convert_to_string(z_tmp);

                key = Z_STRVAL_P(z_tmp);
                key_len = Z_STRLEN_P(z_tmp);
            }

            /* Apply prefix if required */
            key_free = redis_key_prefix(redis_sock, &key, &key_len);

            /* Append this channel */
            redis_cmd_append_sstr(&cmd, key, key_len);

            /* Free key if prefixed */
            if(key_free) efree(key);

            /* Free our temp var if we converted from something other than a string */
            if(z_tmp) {
                zval_dtor(z_tmp);
                efree(z_tmp);
                z_tmp = NULL;
            }
        }

        /* Set return */
        *ret = cmd.c;
        return cmd.len;
    } else if(type == PUBSUB_NUMPAT) {
        return redis_cmd_format_static(ret, "PUBSUB", "s", "NUMPAT",
                                       sizeof("NUMPAT")-1);
    }

    /* Shouldn't ever happen */
    return -1;
}

/*
 * {{{ proto Redis::pubsub("channels", pattern);
 *     proto Redis::pubsub("numsub", Array channels);
 *     proto Redis::pubsub("numpat"); }}}
 */
PHP_METHOD(Redis, pubsub) {
    zval *object;
    RedisSock *redis_sock;
    char *keyword, *cmd;
    int kw_len, cmd_len;
    PUBSUB_TYPE type;
    zval *arg=NULL;

    /* Parse arguments */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|z",
                                    &object, redis_ce, &keyword, &kw_len, &arg)
                                    ==FAILURE)
    {
        RETURN_FALSE;
    }

    /* Validate our sub command keyword, and that we've got proper arguments */
    if(!strncasecmp(keyword, "channels", sizeof("channels"))) {
        /* One (optional) string argument */
        if(arg && Z_TYPE_P(arg) != IS_STRING) {
            RETURN_FALSE;
        }
        type = PUBSUB_CHANNELS;
    } else if(!strncasecmp(keyword, "numsub", sizeof("numsub"))) {
        /* One array argument */
        if(ZEND_NUM_ARGS() < 2 || Z_TYPE_P(arg) != IS_ARRAY ||
           zend_hash_num_elements(Z_ARRVAL_P(arg))==0)
        {
            RETURN_FALSE;
        }
        type = PUBSUB_NUMSUB;
    } else if(!strncasecmp(keyword, "numpat", sizeof("numpat"))) {
        type = PUBSUB_NUMPAT;
    } else {
        /* Invalid keyword */
        RETURN_FALSE;
    }

    /* Grab our socket context object */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0)<0) {
        RETURN_FALSE;
    }

    /* Construct our "PUBSUB" command */
    cmd_len = redis_build_pubsub_cmd(redis_sock, &cmd, type, arg TSRMLS_CC);

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);

    if(type == PUBSUB_NUMSUB) {
        IF_ATOMIC() {
            if(redis_mbulk_reply_zipped_keys_int(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL)<0) {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_mbulk_reply_zipped_keys_int);
    } else {
        IF_ATOMIC() {
            if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL)<0) {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
    }
}

/* Construct an EVAL or EVALSHA command, with option argument array and number of arguments that are keys parameter */
PHP_REDIS_API int
redis_build_eval_cmd(RedisSock *redis_sock, char **ret, char *keyword, char *value, int val_len, zval *args, int keys_count TSRMLS_DC) {
	zval **elem;
	HashTable *args_hash;
	HashPosition hash_pos;
	int cmd_len, args_count = 0;
	int eval_cmd_count = 2;

	/* If we've been provided arguments, we'll want to include those in our eval command */
	if(args != NULL) {
		/* Init our hash array value, and grab the count */
	    args_hash = Z_ARRVAL_P(args);
	    args_count = zend_hash_num_elements(args_hash);

	    /* We only need to process the arguments if the array is non empty */
	    if(args_count >  0) {
	    	/* Header for our EVAL command */
	    	cmd_len = redis_cmd_format_header(ret, keyword, eval_cmd_count + args_count);

	    	/* Now append the script itself, and the number of arguments to treat as keys */
	    	cmd_len = redis_cmd_append_str(ret, cmd_len, value, val_len);
	    	cmd_len = redis_cmd_append_int(ret, cmd_len, keys_count);

			/* Iterate the values in our "keys" array */
			for(zend_hash_internal_pointer_reset_ex(args_hash, &hash_pos);
				zend_hash_get_current_data_ex(args_hash, (void **)&elem, &hash_pos) == SUCCESS;
				zend_hash_move_forward_ex(args_hash, &hash_pos))
			{
				zval *z_tmp = NULL;
				char *key, *old_cmd;
				int key_len, key_free;

				if(Z_TYPE_PP(elem) == IS_STRING) {
					key = Z_STRVAL_PP(elem);
					key_len = Z_STRLEN_PP(elem);
				} else {
					/* Convert it to a string */
					MAKE_STD_ZVAL(z_tmp);
					*z_tmp = **elem;
					zval_copy_ctor(z_tmp);
					convert_to_string(z_tmp);

					key = Z_STRVAL_P(z_tmp);
					key_len = Z_STRLEN_P(z_tmp);
				}

				/* Keep track of the old command pointer */
				old_cmd = *ret;

				/* If this is still a key argument, prefix it if we've been set up to prefix keys */
				key_free = keys_count-- > 0 ? redis_key_prefix(redis_sock, &key, &key_len) : 0;

				/* Append this key to our EVAL command, free our old command */
				cmd_len = redis_cmd_format(ret, "%s$%d" _NL "%s" _NL, *ret, cmd_len, key_len, key, key_len);
				efree(old_cmd);

				/* Free our key, old command if we need to */
				if(key_free) efree(key);

				/* Free our temporary zval (converted from non string) if we've got one */
				if(z_tmp) {
					zval_dtor(z_tmp);
					efree(z_tmp);
				}
			}
	    }
	}

	/* If there weren't any arguments (none passed, or an empty array), construct a standard no args command */
	if(args_count < 1) {
		cmd_len = redis_cmd_format_static(ret, keyword, "sd", value,
                                          val_len, 0);
	}

	/* Return our command length */
	return cmd_len;
}

/* {{{ proto variant Redis::evalsha(string script_sha1, [array keys, int num_key_args])
 */
PHP_METHOD(Redis, evalsha)
{
	zval *object, *args= NULL;
	char *cmd, *sha;
	int cmd_len, sha_len;
	long keys_count = 0;
	RedisSock *redis_sock;

	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|al",
								    &object, redis_ce, &sha, &sha_len, &args, &keys_count) == FAILURE) {
		RETURN_FALSE;
	}

	/* Attempt to grab socket */
	if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	/* Construct our EVALSHA command */
	cmd_len = redis_build_eval_cmd(redis_sock, &cmd, "EVALSHA", sha, sha_len, args, keys_count TSRMLS_CC);

	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
		if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL) < 0) {
			RETURN_FALSE;
		}
	}
	REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

/* {{{ proto variant Redis::eval(string script, [array keys, int num_key_args])
 */
PHP_METHOD(Redis, eval)
{
	zval *object, *args = NULL;
	RedisSock *redis_sock;
	char *script, *cmd = "";
	int script_len, cmd_len;
	long keys_count = 0;

	/* Attempt to parse parameters */
	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|al",
							 &object, redis_ce, &script, &script_len, &args, &keys_count) == FAILURE) {
		RETURN_FALSE;
	}

	/* Attempt to grab socket */
    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    /* Construct our EVAL command */
    cmd_len = redis_build_eval_cmd(redis_sock, &cmd, "EVAL", script, script_len, args, keys_count TSRMLS_CC);

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    IF_ATOMIC() {
    	if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL) < 0) {
    		RETURN_FALSE;
    	}
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

PHP_REDIS_API int
redis_build_script_exists_cmd(char **ret, zval **argv, int argc) {
	/* Our command length and iterator */
	int cmd_len = 0, i;

	/* Start building our command */
	cmd_len = redis_cmd_format_header(ret, "SCRIPT", argc + 1); /* +1 for "EXISTS" */
	cmd_len = redis_cmd_append_str(ret, cmd_len, "EXISTS", 6);

	/* Iterate our arguments */
	for(i=0;i<argc;i++) {
		/* Convert our argument to a string if we need to */
		convert_to_string(argv[i]);

		/* Append this script sha to our SCRIPT EXISTS command */
		cmd_len = redis_cmd_append_str(ret, cmd_len, Z_STRVAL_P(argv[i]), Z_STRLEN_P(argv[i]));
	}

	/* Success */
	return cmd_len;
}

/* {{{ proto status Redis::script('flush')
 * {{{ proto status Redis::script('kill')
 * {{{ proto string Redis::script('load', lua_script)
 * {{{ proto int Reids::script('exists', script_sha1 [, script_sha2, ...])
 */
PHP_METHOD(Redis, script) {
	zval **z_args;
	RedisSock *redis_sock;
	int cmd_len, argc;
	char *cmd;

	/* Attempt to grab our socket */
	if(redis_sock_get(getThis(), &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	/* Grab the number of arguments */
	argc = ZEND_NUM_ARGS();

	/* Allocate an array big enough to store our arguments */
	z_args = emalloc(argc * sizeof(zval*));

	/* Make sure we can grab our arguments, we have a string directive */
	if(zend_get_parameters_array(ht, argc, z_args) == FAILURE ||
	   (argc < 1 || Z_TYPE_P(z_args[0]) != IS_STRING))
	{
		efree(z_args);
		RETURN_FALSE;
	}

	/* Branch based on the directive */
	if(!strcasecmp(Z_STRVAL_P(z_args[0]), "flush") || !strcasecmp(Z_STRVAL_P(z_args[0]), "kill")) {
		// Simple SCRIPT FLUSH, or SCRIPT_KILL command
		cmd_len = redis_cmd_format_static(&cmd, "SCRIPT", "s",
                                          Z_STRVAL_P(z_args[0]),
                                          Z_STRLEN_P(z_args[0]));
	} else if(!strcasecmp(Z_STRVAL_P(z_args[0]), "load")) {
		/* Make sure we have a second argument, and it's not empty.  If it is */
		/* empty, we can just return an empty array (which is what Redis does) */
		if(argc < 2 || Z_TYPE_P(z_args[1]) != IS_STRING || Z_STRLEN_P(z_args[1]) < 1) {
			/* Free our args */
			efree(z_args);
			RETURN_FALSE;
		}

		// Format our SCRIPT LOAD command
		cmd_len = redis_cmd_format_static(&cmd, "SCRIPT", "ss",
                                          "LOAD", 4, Z_STRVAL_P(z_args[1]),
                                          Z_STRLEN_P(z_args[1]));
	} else if(!strcasecmp(Z_STRVAL_P(z_args[0]), "exists")) {
		/* Construct our SCRIPT EXISTS command */
		cmd_len = redis_build_script_exists_cmd(&cmd, &(z_args[1]), argc-1);
	} else {
		/* Unknown directive */
		efree(z_args);
		RETURN_FALSE;
	}

	/* Free our alocated arguments */
	efree(z_args);

	/* Kick off our request */
	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
		if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL) < 0) {
			RETURN_FALSE;
		}
	}
	REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

/* {{{ proto DUMP key
 */
PHP_METHOD(Redis, dump) {
    REDIS_PROCESS_KW_CMD("DUMP", redis_key_cmd, redis_ping_response);
}

/* {{{ proto Redis::DEBUG(string key) */
PHP_METHOD(Redis, debug) {
    zval *object;
    RedisSock *redis_sock;
    char *cmd, *key;
    int cmd_len, key_len, key_free;

    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
                                    &object, redis_ce, &key, &key_len)==FAILURE)
    {
        RETURN_FALSE;
    }

    /* Grab our socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0)<0) {
        RETURN_FALSE;
    }

    /* Prefix key, format command */
    key_free = redis_key_prefix(redis_sock, &key, &key_len TSRMLS_CC);
    cmd_len = redis_cmd_format_static(&cmd, "DEBUG", "ss", "OBJECT", sizeof("OBJECT")-1, key, key_len);
    if(key_free) efree(key);

    /* Kick it off */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    IF_ATOMIC() {
        redis_debug_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_debug_response);
}

/*
 * {{{ proto Redis::restore(ttl, key, value)
 */
PHP_METHOD(Redis, restore) {
    REDIS_PROCESS_KW_CMD("RESTORE", redis_key_long_val_cmd, 
        redis_boolean_response);
}

/*
 * {{{ proto Redis::migrate(host port key dest-db timeout [bool copy, bool replace])
 */
PHP_METHOD(Redis, migrate) {
	zval *object;
	RedisSock *redis_sock;
	char *cmd, *host, *key;
	int cmd_len, host_len, key_len, key_free;
    zend_bool copy=0, replace=0;
	long port, dest_db, timeout;

	/* Parse arguments */
	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oslsll|bb", &object, redis_ce,
									&host, &host_len, &port, &key, &key_len, &dest_db, &timeout,
                                    &copy, &replace) == FAILURE) {
		RETURN_FALSE;
	}

	/* Grabg our socket */
	if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	// Prefix our key if we need to, build our command
	key_free = redis_key_prefix(redis_sock, &key, &key_len);

    /* Construct our command */
    if(copy && replace) {
        cmd_len = redis_cmd_format_static(&cmd, "MIGRATE", "sdsddss",
                                          host, host_len, port, key, key_len,
                                          dest_db, timeout, "COPY",
                                          sizeof("COPY")-1, "REPLACE",
                                          sizeof("REPLACE")-1);
    } else if(copy) {
        cmd_len = redis_cmd_format_static(&cmd, "MIGRATE", "sdsdds",
                                          host, host_len, port, key, key_len,
                                          dest_db, timeout, "COPY",
                                          sizeof("COPY")-1);
    } else if(replace) {
        cmd_len = redis_cmd_format_static(&cmd, "MIGRATE", "sdsdds",
                                          host, host_len, port, key, key_len,
                                          dest_db, timeout, "REPLACE",
                                          sizeof("REPLACE")-1);
    } else {
        cmd_len = redis_cmd_format_static(&cmd, "MIGRATE", "sdsdd",
                                          host, host_len, port, key, key_len,
                                          dest_db, timeout);
    }

    /* Free our key if we prefixed it */
	if(key_free) efree(key);

	/* Kick off our MIGRATE request */
	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
		redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
	}
	REDIS_PROCESS_RESPONSE(redis_boolean_response);
}

/*
 * {{{ proto Redis::_prefix(key)
 */
PHP_METHOD(Redis, _prefix) {
	zval *object;
	RedisSock *redis_sock;
	char *key;
	int key_len;

	/* Parse our arguments */
	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, redis_ce,
								    &key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}
	/* Grab socket */
	if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	/* Prefix our key if we need to */
	if(redis_sock->prefix != NULL && redis_sock->prefix_len > 0) {
		redis_key_prefix(redis_sock, &key, &key_len);
		RETURN_STRINGL(key, key_len, 0);
	} else {
		RETURN_STRINGL(key, key_len, 1);
	}
}

/*
 * {{{ proto Redis::_serialize(value)
 */
PHP_METHOD(Redis, _serialize) {
    zval *object;
    RedisSock *redis_sock;
    zval *z_val;
    char *val;
    int val_len, val_free;

    /* Parse arguments */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oz",
                                    &object, redis_ce, &z_val) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* Grab socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    // Serialize, which will return a value even if no serializer is set
    redis_serialize(redis_sock, z_val, &val, &val_len TSRMLS_CC);

    /* Return serialized value.  Tell PHP to make a copy as some can be interned. */
    RETVAL_STRINGL(val, val_len, 1);
    if(val_free) STR_FREE(val);
}

/*
 * {{{ proto Redis::_unserialize(value)
 */
PHP_METHOD(Redis, _unserialize) {
	zval *object;
	RedisSock *redis_sock;
	char *value;
	int value_len;

	/* Parse our arguments */
	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, redis_ce,
									&value, &value_len) == FAILURE) {
		RETURN_FALSE;
	}
	/* Grab socket */
	if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	/* We only need to attempt unserialization if we have a serializer running */
	if(redis_sock->serializer != REDIS_SERIALIZER_NONE) {
		zval *z_ret = NULL;
		if(redis_unserialize(redis_sock, value, value_len, &z_ret
                             TSRMLS_CC) == 0)
        {
			// Badly formed input, throw an execption
			zend_throw_exception(redis_exception_ce,
                                 "Invalid serialized data, or unserialization error",
                                 0 TSRMLS_CC);
			RETURN_FALSE;
		}
		RETURN_ZVAL(z_ret, 0, 1);
	} else {
		/* Just return the value that was passed to us */
		RETURN_STRINGL(value, value_len, 1);
	}
}

/*
 * {{{ proto Redis::getLastError()
 */
PHP_METHOD(Redis, getLastError) {
	zval *object;
	RedisSock *redis_sock;

	/* Grab our object */
	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, redis_ce) == FAILURE) {
		RETURN_FALSE;
	}
	/* Grab socket */
	if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	/* Return our last error or NULL if we don't have one */
	if(redis_sock->err != NULL && redis_sock->err_len > 0) {
		RETURN_STRINGL(redis_sock->err, redis_sock->err_len, 1);
	} else {
		RETURN_NULL();
	}
}

/*
 * {{{ proto Redis::clearLastError()
 */
PHP_METHOD(Redis, clearLastError) {
	zval *object;
	RedisSock *redis_sock;

	/* Grab our object */
	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, redis_ce) == FAILURE) {
		RETURN_FALSE;
	}
	/* Grab socket */
	if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	/* Clear error message */
	if(redis_sock->err) {
		efree(redis_sock->err);
	}
	redis_sock->err = NULL;

	RETURN_TRUE;
}

/*
 * {{{ proto long Redis::getMode()
 */
PHP_METHOD(Redis, getMode) {
    zval *object;
    RedisSock *redis_sock;

    /* Grab our object */
    if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }

    /* Grab socket */
    if (redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    RETVAL_LONG(redis_sock->mode);
}

/*
 * {{{ proto Redis::time()
 */
PHP_METHOD(Redis, time) {
	zval *object;
	RedisSock *redis_sock;
	char *cmd;
	int cmd_len;

	/* Grab our object */
	if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, redis_ce) == FAILURE) {
		RETURN_FALSE;
	}
	/* Grab socket */
	if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
		RETURN_FALSE;
	}

	/* Build TIME command */
	cmd_len = redis_cmd_format_static(&cmd, "TIME", "");

	/* Execute or queue command */
	REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
	IF_ATOMIC() {
		if(redis_mbulk_reply_raw(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL) < 0) {
			RETURN_FALSE;
		}
	}
	REDIS_PROCESS_RESPONSE(redis_mbulk_reply_raw);
}

/*
 * Introspection stuff
 */

/*
 * {{{ proto Redis::IsConnected
 */
PHP_METHOD(Redis, isConnected) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_TRUE;
    } else {
        RETURN_FALSE;
    }
}

/*
 * {{{ proto Redis::getHost()
 */
PHP_METHOD(Redis, getHost) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_STRING(redis_sock->host, 1);
    } else {
        RETURN_FALSE;
    }
}

/*
 * {{{ proto Redis::getPort()
 */
PHP_METHOD(Redis, getPort) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        /* Return our port */
        RETURN_LONG(redis_sock->port);
    } else {
        RETURN_FALSE;
    }
}

/*
 * {{{ proto Redis::getDBNum
 */
PHP_METHOD(Redis, getDBNum) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        /* Return our db number */
        RETURN_LONG(redis_sock->dbNumber);
    } else {
        RETURN_FALSE;
    }
}

/*
 * {{{ proto Redis::getTimeout
 */
PHP_METHOD(Redis, getTimeout) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_DOUBLE(redis_sock->timeout);
    } else {
        RETURN_FALSE;
    }
}

/*
 * {{{ proto Redis::getReadTimeout
 */
PHP_METHOD(Redis, getReadTimeout) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_DOUBLE(redis_sock->read_timeout);
    } else {
        RETURN_FALSE;
    }
}

/*
 * {{{ proto Redis::getPersistentID
 */
PHP_METHOD(Redis, getPersistentID) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        if(redis_sock->persistent_id != NULL) {
            RETURN_STRING(redis_sock->persistent_id, 1);
        } else {
            RETURN_NULL();
        }
    } else {
        RETURN_FALSE;
    }
}

/*
 * {{{ proto Redis::getAuth
 */
PHP_METHOD(Redis, getAuth) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        if(redis_sock->auth != NULL) {
            RETURN_STRING(redis_sock->auth, 1);
        } else {
            RETURN_NULL();
        }
    } else {
        RETURN_FALSE;
    }
}

/*
 * $redis->client('list');
 * $redis->client('kill', <ip:port>);
 * $redis->client('setname', <name>);
 * $redis->client('getname');
 */
PHP_METHOD(Redis, client) {
    zval *object;
    RedisSock *redis_sock;
    char *cmd, *opt=NULL, *arg=NULL;
    int cmd_len, opt_len, arg_len;

    /* Parse our method parameters */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|s",
        &object, redis_ce, &opt, &opt_len, &arg, &arg_len) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* Grab our socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    /* Build our CLIENT command */
    if(ZEND_NUM_ARGS() == 2) {
        cmd_len = redis_cmd_format_static(&cmd, "CLIENT", "ss",
                                          opt, opt_len, arg, arg_len);
    } else {
        cmd_len = redis_cmd_format_static(&cmd, "CLIENT", "s",
                                          opt, opt_len);
    }

    /* Execute our queue command */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);

    /* We handle CLIENT LIST with a custom response function */
    if(!strncasecmp(opt, "list", 4)) {
        IF_ATOMIC() {
            redis_client_list_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,redis_sock,NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_client_list_reply);
    } else {
        IF_ATOMIC() {
            redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,redis_sock,NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
    }
}

/**
 * Helper to format any combination of SCAN arguments
 */
PHP_REDIS_API int
redis_build_scan_cmd(char **cmd, REDIS_SCAN_TYPE type, char *key, int key_len,
                     int iter, char *pattern, int pattern_len, int count)
{
    char *keyword;
    int arg_count, cmd_len;

    /* Count our arguments +1 for key if it's got one, and + 2 for pattern */
    /* or count given that they each carry keywords with them. */
    arg_count = 1 + (key_len>0) + (pattern_len>0?2:0) + (count>0?2:0);

    /* Turn our type into a keyword */
    switch(type) {
        case TYPE_SCAN:
            keyword = "SCAN";
            break;
        case TYPE_SSCAN:
            keyword = "SSCAN";
            break;
        case TYPE_HSCAN:
            keyword = "HSCAN";
            break;
        case TYPE_ZSCAN:
		default:
            keyword = "ZSCAN";
            break;
    }

    /* Start the command */
    cmd_len = redis_cmd_format_header(cmd, keyword, arg_count);

    /* Add the key in question if we have one */
    if(key_len) {
        cmd_len = redis_cmd_append_str(cmd, cmd_len, key, key_len);
    }

    /* Add our iterator */
    cmd_len = redis_cmd_append_int(cmd, cmd_len, iter);

    /* Append COUNT if we've got it */
    if(count) {
        cmd_len = redis_cmd_append_str(cmd, cmd_len, "COUNT", sizeof("COUNT")-1);
        cmd_len = redis_cmd_append_int(cmd, cmd_len, count);
    }

    /* Append MATCH if we've got it */
    if(pattern_len) {
        cmd_len = redis_cmd_append_str(cmd, cmd_len, "MATCH", sizeof("MATCH")-1);
        cmd_len = redis_cmd_append_str(cmd, cmd_len, pattern, pattern_len);
    }

    /* Return our command length */
    return cmd_len;
}

/**
 * {{{ proto redis::scan(&$iterator, [pattern, [count]])
 */
PHP_REDIS_API void
generic_scan_cmd(INTERNAL_FUNCTION_PARAMETERS, REDIS_SCAN_TYPE type) {
    zval *object, *z_iter;
    RedisSock *redis_sock;
    HashTable *hash;
    char *pattern=NULL, *cmd, *key=NULL;
    int cmd_len, key_len=0, pattern_len=0, num_elements, key_free=0;
    long count=0, iter;

    /* Different prototype depending on if this is a key based scan */
    if(type != TYPE_SCAN) {
        /* Requires a key */
        if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osz/|s!l",
                                        &object, redis_ce, &key, &key_len, &z_iter,
                                        &pattern, &pattern_len, &count)==FAILURE)
        {
            RETURN_FALSE;
        }
    } else {
        /* Doesn't require a key */
        if(zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oz/|s!l",
                                        &object, redis_ce, &z_iter, &pattern, &pattern_len,
                                        &count) == FAILURE)
        {
            RETURN_FALSE;
        }
    }

    /* Grab our socket */
    if(redis_sock_get(object, &redis_sock TSRMLS_CC, 0) < 0) {
        RETURN_FALSE;
    }

    /* Calling this in a pipeline makes no sense */
    IF_NOT_ATOMIC() {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "Can't call SCAN commands in multi or pipeline mode!");
        RETURN_FALSE;
    }

    /* The iterator should be passed in as NULL for the first iteration, but we can treat */
    /* any NON LONG value as NULL for these purposes as we've seperated the variable anyway. */
    if(Z_TYPE_P(z_iter) != IS_LONG || Z_LVAL_P(z_iter)<0) {
        /* Convert to long */
        convert_to_long(z_iter);
        iter = 0;
    } else if(Z_LVAL_P(z_iter)!=0) {
        /* Update our iterator value for the next passthru */
        iter = Z_LVAL_P(z_iter);
    } else {
        /* We're done, back to iterator zero */
        RETURN_FALSE;
    }

    /* Prefix our key if we've got one and we have a prefix set */
    if(key_len) {
        key_free = redis_key_prefix(redis_sock, &key, &key_len);
    }

    /**
     * Redis can return to us empty keys, especially in the case where there are a large
     * number of keys to scan, and we're matching against a pattern.  PHPRedis can be set
     * up to abstract this from the user, by setting OPT_SCAN to REDIS_SCAN_RETRY.  Otherwise
     * we will return empty keys and the user will need to make subsequent calls with
     * an updated iterator.
     */
    do {
        /* Free our previous reply if we're back in the loop.  We know we are
         * if our return_value is an array. */
        if(Z_TYPE_P(return_value) == IS_ARRAY) {
            zval_dtor(return_value);
            ZVAL_NULL(return_value);
        }

        /* Format our SCAN command */
        cmd_len = redis_build_scan_cmd(&cmd, type, key, key_len, (int)iter,
                                   pattern, pattern_len, count);

        /* Execute our command getting our new iterator value */
        REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
        if(redis_sock_read_scan_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                      redis_sock,type,&iter)<0)
        {
            if(key_free) efree(key);
            RETURN_FALSE;
        }

        /* Get the number of elements */
        hash = Z_ARRVAL_P(return_value);
        num_elements = zend_hash_num_elements(hash);
    } while(redis_sock->scan == REDIS_SCAN_RETRY && iter != 0 && num_elements == 0);

    /* Free our key if it was prefixed */
    if(key_free) efree(key);

    /* Update our iterator reference */
    Z_LVAL_P(z_iter) = iter;
}

PHP_METHOD(Redis, scan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_SCAN);
}
PHP_METHOD(Redis, hscan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_HSCAN);
}
PHP_METHOD(Redis, sscan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_SSCAN);
}
PHP_METHOD(Redis, zscan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_ZSCAN);
}

/* 
 * HyperLogLog based commands 
 */

/* {{{ proto Redis::pfAdd(string key, array elements) }}} */
PHP_METHOD(Redis, pfadd) {
    REDIS_PROCESS_CMD(pfadd, redis_1_response);
}

/* {{{ proto Redis::pfCount(string key) }}}*/
PHP_METHOD(Redis, pfcount) {
    REDIS_PROCESS_KW_CMD("PFCOUNT", redis_key_cmd, redis_long_response);
}

/* {{{ proto Redis::pfMerge(string dstkey, array keys) }}}*/
PHP_METHOD(Redis, pfmerge) {
    REDIS_PROCESS_CMD(pfmerge, redis_boolean_response);
}

/* vim: set tabstop=4 softtabstops=4 noexpandtab shiftwidth=4: */
