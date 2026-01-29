#include "server.h"

/* Create a new table data structure. */
table *tableNew(void) {
    table *s = zmalloc(sizeof(*s));
    s->rax = raxNew();
    s->length = 0;
    s->last_id = 0;
    s->first_id= 0;
    s->max_deleted_entry_id = 0;
    s->entries_added = 0;
    s->fields_lp = NULL;
    return s;
}



void tgetfieldsCommand(client *c) {
    robj *o = lookupKeyRead(c->db, c->argv[1]);

    if (!o) {
        addReplyErrorObject(c, shared.nokeyerr);
    } else {
        if (o->type != OBJ_TABLE) {
            addReplyErrorObject(c, shared.wrongtypeerr);
            return;
        }
        table *t = objectGetVal(o);

        writePreparedClient *wpc = prepareClientForFutureWrites(c);
        if (!wpc) return;

        addWritePreparedReplyArrayLen(wpc, lpLength(t->fields_lp));
        unsigned char *pn;
        unsigned char *vstr;
        unsigned int vlen;
        long long lval;
        pn = lpFirst(t->fields_lp);
        while (pn) {
            vstr = lpGetValue(pn, &vlen, &lval);
            if (vstr) {
                addWritePreparedReplyBulkCBuffer(wpc, vstr, vlen);
            } else {
                addWritePreparedReplyBulkLongLong(wpc, lval);
            }
            serverLog(LL_WARNING, "another one %s", lpGetObject(pn));
            pn = lpNext(t->fields_lp, pn);
        } 
    }
}

void tgetfieldsbitsCommand(client *c) {
    robj *o = lookupKeyRead(c->db, c->argv[1]);

    if (!o) {
        addReplyErrorObject(c, shared.nokeyerr);
    } else {
        if (o->type != OBJ_TABLE) {
            addReplyErrorObject(c, shared.wrongtypeerr);
            return;
        }
        table *t = objectGetVal(o);
        long long fieldbitmap;
        getLongLongFromObjectOrReply(c, c->argv[2], &fieldbitmap, NULL);

        writePreparedClient *wpc = prepareClientForFutureWrites(c);
        if (!wpc) return;

        addWritePreparedReplyArrayLen(wpc, lpLength(t->fields_lp));

        int i;
        unsigned char *pn;
        unsigned char *vstr;
        unsigned int vlen;
        long long lval;
        
        pn = lpFirst(t->fields_lp);

        for(i= 0; i<= (lpLength(t->fields_lp)-1); i++) {
            if ((fieldbitmap & (1 << i)) > 0) {
                vstr = lpGetValue(pn, &vlen, &lval);
                if (vstr) {
                    addWritePreparedReplyBulkCBuffer(wpc, vstr, vlen);
                } else {
                    addWritePreparedReplyBulkLongLong(wpc, lval);
                }
            } else {
                addReplyNull(c);
            }
            pn = lpNext(t->fields_lp, pn);
        }
        
    }
}




void tappendCommand(client *c) {
    robj *o = lookupKeyRead(c->db, c->argv[1]);
    if (!o) {
        addReplyErrorObject(c, shared.nokeyerr);
    } else {
        if (((c->argc-1) % 2) == 1) {
            addReplyErrorArity(c);
            return;
        }

        if (o->type != OBJ_TABLE) {
            addReplyErrorObject(c, shared.wrongtypeerr);
            return;
        }

        int j;
        table *t = objectGetVal(o);
        bool all_found = true;
           
        for (j = 3; j < c->argc; j += 2) {
            sds field = objectGetVal(c->argv[j]);
            unsigned char *p = lpFirst(t->fields_lp);

            serverLog(LL_WARNING, "append FIELD: %s", field);

            p = lpFind(t->fields_lp, p, (unsigned char *)field, sdslen(field), 0);
            if (p == NULL) {
                serverLog(LL_WARNING, "append FIELD NOT FOUND: %s", field);
                //addReplyErrorFormat(c, "Field '%s' not found", (char *)field);
                all_found = false;
            } else {
                serverLog(LL_WARNING, "append OK: %s", field);
            }
        }

        if (all_found == true) {
           

            unsigned char *values_lp = lpNew((j-3)/2);
            serverLog(LL_WARNING, "storing fields %i", (j-3)/2);
            //serverLog(LL_WARNING, "storing fields %i", (j-3)/2 );
            // this is wrong, storing fields instead of values, but just a test
            for (j = 3; j < c->argc; j += 2) {
                sds field = objectGetVal(c->argv[j]);
                values_lp = lpAppend(values_lp, (unsigned char *)field, sdslen(field));
                serverLog(LL_WARNING, "stored: %s", (unsigned char *)field );
            }
            /*for (j = 4; j < c->argc; j += 1) {
                values_lp
            }*/
            //raxInsert(t->rax, (unsigned char *)&t->last_id, sizeof(t->last_id), values_lp, NULL);
            raxInsert(t->rax, (unsigned char *)"0", 1, values_lp, NULL);
            t->last_id++;
            t->length++;
            addReplyLongLong(c, t->last_id);
        } else {
            addReplyError(c,"One or more fields not found.");
        }
    }
}

void tappendbitCommand(client *c) {
    robj *o = lookupKeyRead(c->db, c->argv[1]);
    if (!o) {
        addReplyErrorObject(c, shared.nokeyerr);
    } else {
        if (o->type != OBJ_TABLE) {
            addReplyErrorObject(c, shared.wrongtypeerr);
            return;
        }
        table *t = objectGetVal(o);

        long long fieldbitmap;
        int field_pos = 4;
        getLongLongFromObjectOrReply(c, c->argv[3], &fieldbitmap, NULL);
        serverLog(LL_WARNING, "bitmap %lli",fieldbitmap );

        int i;
        int argCounter = 0;
        unsigned char *values_lp = lpNew(1 + (c->argc - field_pos));

        values_lp = lpAppendInteger(values_lp, fieldbitmap);
        serverLog(LL_WARNING, "appending bitmap: %lli", fieldbitmap);
        for(i= 0; i<= (lpLength(t->fields_lp)-1); i++) {
            if (((fieldbitmap & (1 << i)) > 0) && (field_pos + argCounter < c->argc)) {
                sds value = objectGetVal(c->argv[field_pos + argCounter]);
                argCounter++;
                serverLog(LL_WARNING, "Appending field: %i %s", i, value);
                values_lp = lpAppend(values_lp, (unsigned char *)value, sdslen(value));
            } /*else {
                serverLog(LL_WARNING, "ADD FIELD: %i (nil)", i);
            }*/
        }

        raxInsert(t->rax, (unsigned char *)"0", 1, values_lp, NULL);
        t->last_id++;
        t->length++;
        addReplyLongLong(c, t->last_id);
    }
}



void trowCommand(client *c) {
    robj *o = lookupKeyRead(c->db, c->argv[1]);
    if (!o) {
        addReplyErrorObject(c, shared.nokeyerr);
    } else {
        long index;

        if ((getLongFromObjectOrReply(c, c->argv[2], &index, NULL) != C_OK)) return;
        
        table *t = objectGetVal(o);

        void *row = NULL;
        serverLog(LL_WARNING, "rax index %li", index);
        //raxFind(t->rax, (unsigned char *)index, sizeof(index), &row);
        raxFind(t->rax, (unsigned char *)"0", 1, &row);
        serverLog(LL_WARNING, "after raxfind");
        
        if (row != NULL) {
            unsigned char *pn;
            unsigned char *vstr;
            unsigned int vlen;
            long long lval;
            long long valuebitmap;
            int i;
            writePreparedClient *wpc = prepareClientForFutureWrites(c);
            serverLog(LL_WARNING, "before lpFirst");
            pn = lpFirst(row);
            // need to figure out how to assert that the 0th is a long long
            vstr = lpGetValue(pn, &vlen, &valuebitmap);
            serverLog(LL_WARNING, "row bitmap %lli", valuebitmap);

            addWritePreparedReplyArrayLen(wpc, lpLength(t->fields_lp));

            for(i= 0; i<= (lpLength(t->fields_lp)-1); i++) {
                if (((valuebitmap & (1 << i)) > 0)) {
                    pn = lpNext(row, pn);
                    serverLog(LL_WARNING, "getting i %i", i);
                    vstr = lpGetValue(pn, &vlen, &lval);
                    if (vstr) {
                        addWritePreparedReplyBulkCBuffer(wpc, vstr, vlen);
                    } else {
                        addWritePreparedReplyBulkLongLong(wpc, lval);
                    }
                } else {
                    addReplyNull(c);
                    serverLog(LL_WARNING, "skipping i %i", i);
                }
                
            }


            /*while (pn) {
                serverLog(LL_WARNING, "lp loop");
                vstr = lpGetValue(pn, &vlen, &lval);
                if (vstr) {
                    addWritePreparedReplyBulkCBuffer(wpc, vstr, vlen);
                } else {
                    addWritePreparedReplyBulkLongLong(wpc, lval);
                }
                serverLog(LL_WARNING, "from rax->lp %s", lpGetObject(pn));
                pn = lpNext(row, pn);
            } */
            //addReplyError(c,"not implemented");
        } else {
            addReplyError(c,"index not found");
        }

        
    }
}

void tfieldsCommand(client *c) {
    
    /* check if key exists, if it doesn't proceed, if not, error */
    robj *tobj = lookupKeyWrite(c->db, c->argv[1]);
    if (!tobj) {
        int j;
        table *t; 
        robj *o;
        o = createTableObject();
        
        t = objectGetVal(o);
        /* create the fields listpack */
        t->fields_lp = lpNew(c->argc-2);
        
        /* grab the variadic fields */
        for (j = 2; j < c->argc; j++) {
            /* store the field names */
            sds field = objectGetVal(c->argv[j]);
            t->fields_lp = lpAppend(t->fields_lp, (unsigned char *)field, sdslen(field));
            serverLog(LL_WARNING, "ADD FIELD: %s", field);
        }
        serverLog(LL_WARNING, "FIELD Length %lu", lpLength(t->fields_lp));
        dbAdd(c->db, c->argv[1], &o);

        /* return the number of fields */
    
        addReplyLongLong(c, c->argc-2);
    } else {
        /* TFIELDS Can only be called against an empty key... maybe an error? Maybe not? */
        addReplyLongLong(c, -1);
    }
}

void freeTable(table *t) {
    raxFreeWithCallback(t->rax, lpFreeVoid);
    lpFree(t->fields_lp);
    zfree(t);
}

