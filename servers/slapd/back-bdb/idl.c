/* idl.c - ldap id list handling routines */
/* $OpenLDAP$ */
/*
 * Copyright 1998-2002 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <stdio.h>
#include <ac/string.h>

#include "back-bdb.h"
#include "idl.h"

#define IDL_MAX(x,y)	( x > y ? x : y )
#define IDL_MIN(x,y)	( x < y ? x : y )

#define IDL_CMP(x,y)	( x < y ? -1 : ( x > y ? 1 : 0 ) )

#if IDL_DEBUG > 0
static void idl_check( ID *ids )
{
	if( BDB_IDL_IS_RANGE( ids ) ) {
		assert( BDB_IDL_RANGE_FIRST(ids) <= BDB_IDL_RANGE_LAST(ids) );
	} else {
		ID i;
		for( i=1; i < ids[0]; i++ ) {
			assert( ids[i+1] > ids[i] );
		}
	}
}

#if IDL_DEBUG > 1
static void idl_dump( ID *ids )
{
	if( BDB_IDL_IS_RANGE( ids ) ) {
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_INFO, "IDL: range (%ld - %ld)\n",
			(long) BDB_IDL_RANGE_FIRST( ids ),
			(long) BDB_IDL_RANGE_LAST( ids ) ));
#else
		Debug( LDAP_DEBUG_ANY,
			"IDL: range ( %ld - %ld )\n",
			(long) BDB_IDL_RANGE_FIRST( ids ),
			(long) BDB_IDL_RANGE_LAST( ids ) );
#endif

	} else {
		ID i;
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_INFO, "IDL: size %ld",
			(long) ids[0] ));
#else
		Debug( LDAP_DEBUG_ANY, "IDL: size %ld", (long) ids[0], 0, 0 );
#endif

		for( i=1; i<=ids[0]; i++ ) {
			if( i % 16 == 1 ) {
				Debug( LDAP_DEBUG_ANY, "\n", 0, 0, 0 );
			}
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_INFO, "%02lx",(long)ids[i] ));
#else
			Debug( LDAP_DEBUG_ANY, "  %02lx", (long) ids[i], 0, 0 );
#endif
		}

		Debug( LDAP_DEBUG_ANY, "\n", 0, 0, 0 );
	}

	idl_check( ids );
}
#endif /* IDL_DEBUG > 1 */
#endif /* IDL_DEBUG > 0 */

unsigned bdb_idl_search( ID *ids, ID id )
{
#define IDL_BINARY_SEARCH 1
#ifdef IDL_BINARY_SEARCH
	/*
	 * binary search of id in ids
	 * if found, returns position of id
	 * if not found, returns first postion greater than id
	 */
	unsigned base = 0;
	unsigned cursor = 0;
	int val = 0;
	unsigned n = ids[0];

#if IDL_DEBUG > 0
	idl_check( ids );
#endif

	while( 0 < n ) {
		int pivot = n >> 1;
		cursor = base + pivot;
		val = IDL_CMP( id, ids[cursor + 1] );

		if( val < 0 ) {
			n = pivot;

		} else if ( val > 0 ) {
			base = cursor + 1;
			n -= pivot + 1;

		} else {
			return cursor + 1;
		}
	}
	
	if( val > 0 ) {
		return cursor + 2;
	} else {
		return cursor + 1;
	}

#else
	/* (reverse) linear search */
	int i;

#if IDL_DEBUG > 0
	idl_check( ids );
#endif

	for( i=ids[0]; i; i-- ) {
		if( id > ids[i] ) {
			break;
		}
	}

	return i+1;
#endif
}

int bdb_idl_insert( ID *ids, ID id )
{
	unsigned x = bdb_idl_search( ids, id );

#if IDL_DEBUG > 1
#ifdef NEW_LOGGING
	LDAP_LOG(( "idl", LDAP_LEVEL_DETAIL1, "insert: %04lx at %d\n",
			(long) id, x ));
#else
	Debug( LDAP_DEBUG_ANY, "insert: %04lx at %d\n", (long) id, x, 0 );
	idl_dump( ids );
#endif
#elif IDL_DEBUG > 0
	idl_check( ids );
#endif

	assert( x > 0 );

	if( x < 1 ) {
		/* internal error */
		return -2;
	}

	if ( x <= ids[0] && ids[x] == id ) {
		/* duplicate */
		return -1;
	}

	if ( ++ids[0] >= BDB_IDL_DB_MAX ) {
		if( id < ids[1] ) {
			ids[1] = id;
			ids[2] = ids[ids[0]-1];
		} else if ( ids[ids[0]-1] < id ) {
			ids[2] = id;
		} else {
			ids[2] = ids[ids[0]-1];
		}
		ids[0] = NOID;
	
	} else {
		/* insert id */
		AC_MEMCPY( &ids[x+1], &ids[x], (ids[0]-x) * sizeof(ID) );
		ids[x] = id;
	}

#if IDL_DEBUG > 1
	idl_dump( ids );
#elif IDL_DEBUG > 0
	idl_check( ids );
#endif

	return 0;
}

static int idl_delete( ID *ids, ID id )
{
	unsigned x = bdb_idl_search( ids, id );

#if IDL_DEBUG > 1
#ifdef NEW_LOGGING
	LDAP_LOG(( "idl", LDAP_LEVEL_DETAIL1, "delete: %04lx at %d\n",
			(long) id, x ));
#else
	Debug( LDAP_DEBUG_ANY, "delete: %04lx at %d\n", (long) id, x, 0 );
	idl_dump( ids );
#endif
#elif IDL_DEBUG > 0
	idl_check( ids );
#endif

	assert( x > 0 );

	if( x <= 0 ) {
		/* internal error */
		return -2;
	}

	if( x > ids[0] || ids[x] != id ) {
		/* not found */
		return -1;

	} else if ( --ids[0] == 0 ) {
		if( x != 1 ) {
			return -3;
		}

	} else {
		AC_MEMCPY( &ids[x], &ids[x+1], (1+ids[0]-x) * sizeof(ID) );
	}

#if IDL_DEBUG > 1
	idl_dump( ids );
#elif IDL_DEBUG > 0
	idl_check( ids );
#endif

	return 0;
}

int
bdb_idl_fetch_key(
	BackendDB	*be,
	DB			*db,
	DB_TXN		*tid,
	DBT			*key,
	ID			*ids )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int rc;
	DBT data;

	assert( ids != NULL );

	DBTzero( &data );

#ifdef BDB_IDL_MULTI
	{
		DBC *cursor;
		ID buf[BDB_PAGESIZE*4];
		ID *i;
		void *ptr;
		size_t len;
		int rc2;
		int flags = bdb->bi_db_opflags | DB_MULTIPLE;
		data.data = buf;
		data.ulen = sizeof(buf);
		data.flags = DB_DBT_USERMEM;

		if ( tid )
			flags |= DB_RMW;

		rc = db->cursor( db, tid, &cursor, bdb->bi_db_opflags );
		if( rc != 0 ) {
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_fetch_key: cursor failed: %s (%d)\n", db_strerror(rc), rc ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_fetch_key: "
				"cursor failed: %s (%d)\n", db_strerror(rc), rc, 0 );
#endif
			return rc;
		}
		rc = cursor->c_get( cursor, key, &data, flags | DB_SET );
		if (rc == 0) {
			i = ids;
			while (rc == 0) {
				u_int8_t *j;

				DB_MULTIPLE_INIT( ptr, &data );
				while (ptr) {
					DB_MULTIPLE_NEXT(ptr, &data, j, len);
					if (j) {
						++i;
						AC_MEMCPY( i, j, sizeof(ID) );
					}
				}
				rc = cursor->c_get( cursor, key, &data, flags | DB_NEXT_DUP );
			}
			if ( rc == DB_NOTFOUND ) rc = 0;
			ids[0] = i - ids;
			/* On disk, a range is denoted by 0 in the first element */
			if (ids[1] == 0) {
				if (ids[0] != BDB_IDL_RANGE_SIZE) {
#ifdef NEW_LOGGING
					LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "=> bdb_idl_fetch_key: range size mismatch: expected %ld, got %ld\n", BDB_IDL_RANGE_SIZE, ids[0] ));
#else
					Debug( LDAP_DEBUG_ANY, "=> bdb_idl_fetch_key: "
						"range size mismatch: expected %ld, got %ld\n",
						BDB_IDL_RANGE_SIZE, ids[0], 0 );
#endif
					cursor->c_close( cursor );
					return -1;
				}
				BDB_IDL_RANGE( ids, ids[2], ids[3] );
			}
			data.size = BDB_IDL_SIZEOF(ids);
		}
		rc2 = cursor->c_close( cursor );
		if (rc2) {
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_fetch_key: close failed: %s (%d)\n", db_strerror(rc2), rc2 ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_fetch_key: "
				"close failed: %s (%d)\n", db_strerror(rc2), rc2, 0 );
#endif
			return rc2;
		}
	}
#else
	data.data = ids;
	data.ulen = BDB_IDL_UM_SIZEOF;
	data.flags = DB_DBT_USERMEM;
	/* fetch it */
	rc = db->get( db, tid, key, &data, bdb->bi_db_opflags );
#endif

	if( rc == DB_NOTFOUND ) {
		return rc;

	} else if( rc != 0 ) {
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_fetch_key: get failed: %s (%d)\n", db_strerror(rc), rc ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_fetch_key: "
			"get failed: %s (%d)\n",
			db_strerror(rc), rc, 0 );
#endif
		return rc;

	} else if ( data.size == 0 || data.size % sizeof( ID ) ) {
		/* size not multiple of ID size */
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_fetch_key: odd size: expected %ld multiple, got %ld\n", (long) sizeof( ID ), (long) data.size ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_fetch_key: "
			"odd size: expected %ld multiple, got %ld\n",
			(long) sizeof( ID ), (long) data.size, 0 );
#endif
		return -1;

	} else if ( data.size != BDB_IDL_SIZEOF(ids) ) {
		/* size mismatch */
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_fetch_key: get size mismatch: expected %ld, got %ld\n", (long) ((1 + ids[0]) * sizeof( ID )), (long) data.size ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_fetch_key: "
			"get size mismatch: expected %ld, got %ld\n",
			(long) ((1 + ids[0]) * sizeof( ID )), (long) data.size, 0 );
#endif
		return -1;
	}

	return rc;
}

int
bdb_idl_insert_key(
	BackendDB	*be,
	DB			*db,
	DB_TXN		*tid,
	DBT			*key,
	ID			id )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int	rc;
	DBT data;
#ifndef BDB_IDL_MULTI
	ID ids[BDB_IDL_DB_SIZE];
#endif

#if 0
	/* for printable keys only */
#ifdef NEW_LOGGING
	LDAP_LOG(( "idl", LDAP_LEVEL_ARGS, "bdb_idl_insert_key: %s %ld\n", (char *)key->data, (long) id ));
#else
	Debug( LDAP_DEBUG_ARGS,
		"=> bdb_idl_insert_key: %s %ld\n",
		(char *)key->data, (long) id, 0 );
#endif
#endif

	assert( id != NOID );

	DBTzero( &data );
#ifdef BDB_IDL_MULTI
	{
		DBC *cursor;
		ID lo, hi, tmp;
		char *err;

		data.size = sizeof( ID );
		data.ulen = data.size;
		data.flags = DB_DBT_USERMEM;

		rc = db->cursor( db, tid, &cursor, bdb->bi_db_opflags );
		if ( rc != 0 ) {
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_insert_key: cursor failed: %s (%d)\n", db_strerror(rc), rc ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_insert_key: "
				"cursor failed: %s (%d)\n", db_strerror(rc), rc, 0 );
#endif
			return rc;
		}
		data.data = &tmp;
		/* Fetch the first data item for this key, to see if it
		 * exists and if it's a range.
		 */
		rc = cursor->c_get( cursor, key, &data, DB_SET | DB_RMW );
		err = "c_get";
		if ( rc == 0 ) {
			if ( tmp != 0 ) {
				/* not a range, count the number of items */
				db_recno_t count;
				rc = cursor->c_count( cursor, &count, 0 );
				if ( rc != 0 ) {
					err = "c_count";
					goto fail;
				}
				if ( count >= BDB_IDL_DB_SIZE ) {
				/* No room, convert to a range */
					lo = tmp;
					data.data = &hi;
					rc = cursor->c_get( cursor, key, &data, DB_LAST );
					if ( rc != 0 ) {
						err = "c_get last";
						goto fail;
					}
					if ( id < lo )
						lo = id;
					else if ( id > hi )
						hi = id;
					rc = db->del( db, tid, key, 0 );
					if ( rc != 0 ) {
						err = "del";
						goto fail;
					}
					data.data = &id;
					id = 0;
					rc = cursor->c_put( cursor, key, &data, DB_KEYFIRST );
					if ( rc != 0 ) {
						err = "c_put 0";
						goto fail;
					}
					id = lo;
					rc = cursor->c_put( cursor, key, &data, DB_KEYLAST );
					if ( rc != 0 ) {
						err = "c_put lo";
						goto fail;
					}
					id = hi;
					rc = cursor->c_put( cursor, key, &data, DB_KEYLAST );
					if ( rc != 0 ) {
						err = "c_put hi";
						goto fail;
					}
				} else {
				/* There's room, just store it */
					goto put1;
				}
			} else {
				/* It's a range, see if we need to rewrite
				 * the boundaries
				 */
				hi = id;
				data.data = &lo;
				rc = cursor->c_get( cursor, key, &data, DB_NEXT_DUP );
				if ( rc != 0 ) {
					err = "c_get lo";
					goto fail;
				}
				if ( id > lo ) {
					data.data = &hi;
					rc = cursor->c_get( cursor, key, &data, DB_NEXT_DUP );
					if ( rc != 0 ) {
						err = "c_get hi";
						goto fail;
					}
				}
				if ( id < lo || id > hi ) {
					/* Delete the current lo/hi */
					rc = cursor->c_del( cursor, 0 );
					if ( rc != 0 ) {
						err = "c_del";
						goto fail;
					}
					data.data = &id;
					rc = cursor->c_put( cursor, key, &data, DB_KEYFIRST );
					if ( rc != 0 ) {
						err = "c_put lo/hi";
						goto fail;
					}
				}
			}
		} else if ( rc == DB_NOTFOUND ) {
put1:			data.data = &id;
			rc = cursor->c_put( cursor, key, &data, DB_KEYFIRST );
			/* Don't worry if it's already there */
			if ( rc != 0 && rc != DB_KEYEXIST ) {
				err = "c_put id";
				goto fail;
			}
		} else {
			/* initial c_get failed, nothing was done */
fail:
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_insert_key: %s failed: %s (%d)\n", err, db_strerror(rc), rc ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_insert_key: "
				"%s failed: %s (%d)\n", err, db_strerror(rc), rc );
#endif
			cursor->c_close( cursor );
			return rc;
		}
		rc = cursor->c_close( cursor );
	}
#else	/* !BDB_IDL_MULTI */
	data.data = ids;
	data.ulen = sizeof ids;
	data.flags = DB_DBT_USERMEM;

	/* fetch the key for read/modify/write */
	rc = db->get( db, tid, key, &data, DB_RMW | bdb->bi_db_opflags );

	if( rc == DB_NOTFOUND ) {
		ids[0] = 1;
		ids[1] = id;
		data.size = 2 * sizeof( ID );

	} else if ( rc != 0 ) {
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_insert_key: get failed: %s (%d)\n", db_strerror(rc), rc ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_insert_key: "
			"get failed: %s (%d)\n",
			db_strerror(rc), rc, 0 );
#endif
		return rc;

	} else if ( data.size == 0 || data.size % sizeof( ID ) ) {
		/* size not multiple of ID size */
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_insert_key: odd size: expected %ld multiple, got %ld\n", (long) sizeof( ID ), (long) data.size ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_insert_key: "
			"odd size: expected %ld multiple, got %ld\n",
			(long) sizeof( ID ), (long) data.size, 0 );
#endif
		return -1;
	
	} else if ( data.size != BDB_IDL_SIZEOF(ids) ) {
		/* size mismatch */
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_insert_key: odd size: expected %ld multiple, got %ld\n", (long) ((1 + ids[0]) * sizeof( ID )), (long) data.size ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_insert_key: "
			"get size mismatch: expected %ld, got %ld\n",
			(long) ((1 + ids[0]) * sizeof( ID )), (long) data.size, 0 );
#endif
		return -1;

	} else if ( BDB_IDL_IS_RANGE(ids) ) {
		if( id < ids[1] ) {
			ids[1] = id;
		} else if ( ids[2] > id ) {
			ids[2] = id;
		} else {
			return 0;
		}

	} else {
		rc = bdb_idl_insert( ids, id );

		if( rc == -1 ) {
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_DETAIL1, "bdb_idl_insert_key: dup\n" ));
#else
			Debug( LDAP_DEBUG_TRACE, "=> bdb_idl_insert_key: dup\n",
				0, 0, 0 );
#endif
			return 0;
		}
		if( rc != 0 ) {
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_insert_key: insert failed: (%d)\n", rc ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_insert_key: "
				"bdb_idl_insert failed (%d)\n",
				rc, 0, 0 );
#endif
			
			return rc;
		}

		data.size = BDB_IDL_SIZEOF( ids );
	}

	/* store the key */
	rc = db->put( db, tid, key, &data, 0 );
#endif
	if( rc == DB_KEYEXIST ) rc = 0;

	if( rc != 0 ) {
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_insert_key: put failed: (%d)\n", db_strerror(rc), rc ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_insert_key: "
			"put failed: %s (%d)\n",
			db_strerror(rc), rc, 0 );
#endif
	}
	return rc;
}

int
bdb_idl_delete_key(
	BackendDB	*be,
	DB			*db,
	DB_TXN		*tid,
	DBT			*key,
	ID			id )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int	rc;
	DBT data;
#ifndef BDB_IDL_MULTI
	ID ids[BDB_IDL_DB_SIZE];
#endif

#if 0
	/* for printable keys only */
#ifdef NEW_LOGGING
	LDAP_LOG(( "idl", LDAP_LEVEL_ARGS, "bdb_idl_delete_key: %s %ld\n", (char *)key->data, (long) id ));
#else
	Debug( LDAP_DEBUG_ARGS,
		"=> bdb_idl_delete_key: %s %ld\n",
		(char *)key->data, (long) id, 0 );
#endif
#endif

	assert( id != NOID );

	DBTzero( &data );
#ifdef BDB_IDL_MULTI
	{
		DBC *cursor;
		ID lo, hi, tmp;
		char *err;

		data.data = &tmp;
		data.size = sizeof( id );
		data.ulen = data.size;
		data.flags = DB_DBT_USERMEM;

		rc = db->cursor( db, tid, &cursor, bdb->bi_db_opflags );
		if ( rc != 0 ) {
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: cursor failed: %s (%d)\n", db_strerror(rc), rc ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_delete_key: "
				"cursor failed: %s (%d)\n", db_strerror(rc), rc, 0 );
#endif
			return rc;
		}
		/* Fetch the first data item for this key, to see if it
		 * exists and if it's a range.
		 */
		rc = cursor->c_get( cursor, key, &data, DB_SET | DB_RMW );
		err = "c_get";
		if ( rc == 0 ) {
			if ( tmp != 0 ) {
				/* Not a range, just delete it */
				data.data = &id;
				rc = cursor->c_get( cursor, key, &data, 
					DB_GET_BOTH | DB_RMW  );
				if ( rc == 0 ) {
					rc = cursor->c_del( cursor, 0 );
					if ( rc != 0 ) {
						err = "c_del id";
						goto fail;
					}
				}
			} else {
				/* It's a range, see if we need to rewrite
				 * the boundaries
				 */
				hi = 0;
				data.data = &lo;
				rc = cursor->c_get( cursor, key, &data, DB_NEXT_DUP );
				if ( rc != 0 ) {
					err = "c_get lo";
					goto fail;
				}
				if ( id > lo ) {
					data.data = &hi;
					rc = cursor->c_get( cursor, key, &data, DB_NEXT_DUP );
					if ( rc != 0 ) {
						err = "c_get hi";
						goto fail;
					}
				}
				if ( id == lo || id == hi ) {
					if ( id == lo ) {
						id++;
						lo = id;
					} else if ( id == hi ) {
						id--;
						hi = id;
					}
					if ( lo >= hi ) {
					/* The range has collapsed... */
						rc = db->del( db, tid, key, 0 );
						if ( rc != 0 ) {
							err = "del";
							goto fail;
						}
					} else {
						rc = cursor->c_del( cursor, 0 );
						if ( rc != 0 ) {
							err = "c_del";
							goto fail;
						}
					}
					if ( lo <= hi ) {
						data.data = &id;
						rc = cursor->c_put( cursor, key, &data, DB_KEYFIRST );
						if ( rc != 0 ) {
							err = "c_put lo/hi";
							goto fail;
						}
					}
				}
			}
		} else {
			/* initial c_get failed, nothing was done */
fail:
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: %s failed: %s (%d)\n", err, db_strerror(rc), rc ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_delete_key: "
				"%s failed: %s (%d)\n", err, db_strerror(rc), rc );
#endif
			cursor->c_close( cursor );
			return rc;
		}
		rc = cursor->c_close( cursor );
	}
#else
	data.data = ids;
	data.ulen = sizeof( ids );
	data.flags = DB_DBT_USERMEM;

	/* fetch the key for read/modify/write */
	rc = db->get( db, tid, key, &data, DB_RMW | bdb->bi_db_opflags );

	if ( rc != 0 ) {
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: get failed: %s (%d)\n", db_strerror(rc), rc ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_delete_key: "
			"get failed: %s (%d)\n",
			db_strerror(rc), rc, 0 );
#endif
		return rc;

	} else if ( data.size == 0 || data.size % sizeof( ID ) ) {
		/* size not multiple of ID size */
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: odd size: expected: %ld multiple, got %ld\n", (long) sizeof( ID ), (long) data.size ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_delete_key: "
			"odd size: expected %ld multiple, got %ld\n",
			(long) sizeof( ID ), (long) data.size, 0 );
#endif
		return -1;
	
	} else if ( BDB_IDL_IS_RANGE(ids) ) {
		return 0;

	} else if ( data.size != (1 + ids[0]) * sizeof( ID ) ) {
		/* size mismatch */
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: get size mismatch: expected: %ld, got %ld\n", (long) ((1 + ids[0]) * sizeof( ID )), (long) data.size ));
#else
		Debug( LDAP_DEBUG_ANY, "=> bdb_idl_delete_key: "
			"get size mismatch: expected %ld, got %ld\n",
			(long) ((1 + ids[0]) * sizeof( ID )), (long) data.size, 0 );
#endif
		return -1;

	} else {
		rc = idl_delete( ids, id );

		if( rc != 0 ) {
#ifdef NEW_LOGGING
			LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: delete failed: (%d)\n", rc ));
#else
			Debug( LDAP_DEBUG_ANY, "=> bdb_idl_delete_key: "
				"idl_delete failed (%d)\n",
				rc, 0, 0 );
#endif
			return rc;
		}

		if( ids[0] == 0 ) {
			/* delete the key */
			rc = db->del( db, tid, key, 0 );
			if( rc != 0 ) {
#ifdef NEW_LOGGING
				LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: delete failed: %s (%d)\n", db_strerror(rc), rc ));
#else
				Debug( LDAP_DEBUG_ANY, "=> bdb_idl_delete_key: "
					"delete failed: %s (%d)\n",
					db_strerror(rc), rc, 0 );
#endif
			}
			return rc;
		}

		data.size = (ids[0]+1) * sizeof( ID );
	}

	/* store the key */
	rc = db->put( db, tid, key, &data, 0 );

#endif /* BDB_IDL_MULTI */

	if( rc != 0 ) {
#ifdef NEW_LOGGING
		LDAP_LOG(( "idl", LDAP_LEVEL_ERR, "bdb_idl_delete_key: put failed: %s (%d)\n", db_strerror(rc), rc ));
#else
		Debug( LDAP_DEBUG_ANY,
			"=> bdb_idl_delete_key: put failed: %s (%d)\n",
			db_strerror(rc), rc, 0 );
#endif
	}

	return rc;
}


/*
 * idl_intersection - return a = a intersection b
 */
int
bdb_idl_intersection(
	ID *a,
	ID *b )
{
	ID ida, idb;
	ID idmax, idmin;
	ID cursora = 0, cursorb = 0, cursorc;
	int swap = 0;

	if ( BDB_IDL_IS_ZERO( a ) || BDB_IDL_IS_ZERO( b ) ) {
		a[0] = 0;
		return 0;
	}

	idmin = IDL_MAX( BDB_IDL_FIRST(a), BDB_IDL_FIRST(b) );
	idmax = IDL_MIN( BDB_IDL_LAST(a), BDB_IDL_LAST(b) );
	if ( idmin > idmax ) {
		a[0] = 0;
		return 0;
	} else if ( idmin == idmax ) {
		a[0] = 1;
		a[1] = idmin;
		return 0;
	}

	if ( BDB_IDL_IS_RANGE( a ) && BDB_IDL_IS_RANGE(b) ) {
		a[1] = idmin;
		a[2] = idmax;
		return 0;
	}

	if ( BDB_IDL_IS_RANGE( a ) ) {
		ID *tmp = a;
		a = b;
		b = tmp;
		swap = 1;
	}

	if ( BDB_IDL_IS_RANGE( b ) && BDB_IDL_FIRST( b ) <= idmin &&
		BDB_IDL_LAST( b ) >= idmax) {
		if (idmax - idmin + 1 == a[0])
		{
			a[0] = NOID;
			a[1] = idmin;
			a[2] = idmax;
		}
		goto done;
	}

	ida = bdb_idl_first( a, &cursora );
	idb = bdb_idl_first( b, &cursorb );
	cursorc = 0;

	while( ida < idmin )
		ida = bdb_idl_next( a, &cursora );
	while( idb < idmin )
		idb = bdb_idl_next( b, &cursorb );

	while( ida <= idmax || idb <= idmax ) {
		if( ida == idb ) {
			a[++cursorc] = ida;
			ida = bdb_idl_next( a, &cursora );
			idb = bdb_idl_next( b, &cursorb );
		} else if ( ida < idb ) {
			ida = bdb_idl_next( a, &cursora );
		} else {
			idb = bdb_idl_next( b, &cursorb );
		}
	}
	a[0] = cursorc;
done:
	if (swap)
		BDB_IDL_CPY( b, a );

	return 0;
}


/*
 * idl_union - return a = a union b
 */
int
bdb_idl_union(
	ID	*a,
	ID	*b )
{
	ID ida, idb;
	ID cursora = 0, cursorb = 0, cursorc;

	if ( BDB_IDL_IS_ZERO( b ) ) {
		return 0;
	}

	if ( BDB_IDL_IS_ZERO( a ) ) {
		BDB_IDL_CPY( a, b );
		return 0;
	}

	if ( BDB_IDL_IS_RANGE( a ) || BDB_IDL_IS_RANGE(b) ) {
over:		a[1] = IDL_MIN( BDB_IDL_FIRST(a), BDB_IDL_FIRST(b) );
		a[2] = IDL_MAX( BDB_IDL_LAST(a), BDB_IDL_LAST(b) );
		return 0;
	}

	ida = bdb_idl_first( a, &cursora );
	idb = bdb_idl_first( b, &cursorb );

	cursorc = b[0];

	/* The distinct elements of a are cat'd to b */
	while( ida != NOID || idb != NOID ) {
		if ( ida < idb ) {
			if( ++cursorc > BDB_IDL_UM_MAX ) {
				a[0] = NOID;
				goto over;
			}
			b[cursorc] = ida;
			ida = bdb_idl_next( a, &cursora );

		} else {
			if ( ida == idb )
				ida = bdb_idl_next( a, &cursora );
			idb = bdb_idl_next( b, &cursorb );
		}
	}

	/* b is copied back to a in sorted order */
	a[0] = cursorc;
	cursora = 1;
	cursorb = 1;
	cursorc = b[0]+1;
	while (cursorb <= b[0] || cursorc <= a[0]) {
		if (cursorc > a[0])
			idb = NOID;
		else
			idb = b[cursorc];
		if (b[cursorb] < idb)
			a[cursora++] = b[cursorb++];
		else {
			a[cursora++] = idb;
			cursorc++;
		}
	}

	return 0;
}


#if 0
/*
 * bdb_idl_notin - return a intersection ~b (or a minus b)
 */
int
bdb_idl_notin(
	ID	*a,
	ID	*b,
	ID *ids )
{
	ID ida, idb;
	ID cursora = 0, cursorb = 0;

	if( BDB_IDL_IS_ZERO( a ) ||
		BDB_IDL_IS_ZERO( b ) ||
		BDB_IDL_IS_RANGE( b ) )
	{
		BDB_IDL_CPY( ids, a );
		return 0;
	}

	if( BDB_IDL_IS_RANGE( a ) ) {
		BDB_IDL_CPY( ids, a );
		return 0;
	}

	ida = bdb_idl_first( a, &cursora ),
	idb = bdb_idl_first( b, &cursorb );

	ids[0] = 0;

	while( ida != NOID ) {
		if ( idb == NOID ) {
			/* we could shortcut this */
			ids[++ids[0]] = ida;
			ida = bdb_idl_next( a, &cursora );

		} else if ( ida < idb ) {
			ids[++ids[0]] = ida;
			ida = bdb_idl_next( a, &cursora );

		} else if ( ida > idb ) {
			idb = bdb_idl_next( b, &cursorb );

		} else {
			ida = bdb_idl_next( a, &cursora );
			idb = bdb_idl_next( b, &cursorb );
		}
	}

	return 0;
}
#endif

ID bdb_idl_first( ID *ids, ID *cursor )
{
	ID pos;

	if ( ids[0] == 0 ) {
		*cursor = NOID;
		return NOID;
	}

	if ( BDB_IDL_IS_RANGE( ids ) ) {
		if( *cursor < ids[1] ) {
			*cursor = ids[1];
		}
		return *cursor;
	}

	if ( *cursor == 0 )
		pos = 1;
	else
		pos = bdb_idl_search( ids, *cursor );

	if( pos > ids[0] ) {
		return NOID;
	}

	*cursor = pos;
	return ids[pos];
}

ID bdb_idl_next( ID *ids, ID *cursor )
{
	if ( BDB_IDL_IS_RANGE( ids ) ) {
		if( ids[2] < ++(*cursor) ) {
			return NOID;
		}
		return *cursor;
	}

	if ( ++(*cursor) <= ids[0] ) {
		return ids[*cursor];
	}

	return NOID;
}
