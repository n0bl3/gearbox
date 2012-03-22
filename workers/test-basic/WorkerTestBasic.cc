// Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
// Copyrights licensed under the New BSD License. See the accompanying LICENSE file for terms.

#include "WorkerTestBasic.h"

#include <gearbox/core/util.h>
#include <gearbox/core/Errors.h>

#include <glob.h>

#include <boost/filesystem/operations.hpp>
namespace bfs=boost::filesystem;

static const std::string DBDIR(LOCALSTATEDIR "/gearbox/db/test-basic/");

namespace Gearbox {
    WorkerTestBasic::WorkerTestBasic(const std::string & config) : super(config) {
        WORKER_REGISTER(WorkerTestBasic, do_get_testbasic_thing_v1);
        WORKER_REGISTER(WorkerTestBasic, do_put_testbasic_thing_v1);
        WORKER_REGISTER(WorkerTestBasic, do_post_testbasic_thing_v1);
        WORKER_REGISTER(WorkerTestBasic, do_delete_testbasic_thing_v1);
    }

    Worker::response_t
    WorkerTestBasic::do_get_testbasic_thing_v1( const Job & job, JobResponse & resp ) {
        
        if( job.environ().has_key("TestBasic") ) {
            resp.add_header("TestBasic", job.environ()["TestBasic"]);
        }
        
        if( job.arguments().empty() ) {
            // index get
            
            Json out;
            glob_t globbuf;

            // glob out all things in our db
            glob( std::string(DBDIR + "*").c_str(), 0, NULL, &globbuf);

            // set things to an empty array in case our glob does not match anything
            out["things"] = Json::Array();
            
            unsigned int limit = globbuf.gl_pathc;
            if( job.query_params().has_key("_count") ) {
                limit = boost::lexical_cast<unsigned int>(job.query_params()["_count"]);
            }

            // set populate all the thing names for the index
            for( unsigned int i=0; i < limit; i++ ) {
                if( job.matrix_arguments().has_key("_expand")
                    && job.matrix_arguments()["_expand"] == "1" ) {
                    out["things"][i].parse( slurp(globbuf.gl_pathv[i]) );
                }
                else {
                    out["things"][i] = bfs::basename(globbuf.gl_pathv[i]);
                }
            }
        
            globfree(&globbuf);

            // set the output content
            resp.content( out.serialize() );
        }
        else {
            const std::string & name = job.arguments()[0];
            // instance get
            if( bfs::exists( DBDIR + name ) ) {
                // set output to contests of the file on disk
                resp.content( slurp(DBDIR + name) );
            }
            else {
                gbTHROW( ERR_NOT_FOUND("thing \"" + name + "\" not found") );
            }
        }
        return WORKER_SUCCESS;
    }

    Worker::response_t
    WorkerTestBasic::do_put_testbasic_thing_v1( const Job & job, JobResponse & resp ) {
        // async message, so update status to let user know we are processing
        resp.status()->progress(10);
        resp.status()->add_message("processing");

        if( job.arguments().empty() )
            gbTHROW( ERR_BAD_REQUEST("missing required resource name") );
        
        if( ! job.json_content().hasKey("id") )
            gbTHROW( ERR_BAD_REQUEST("missing required \"id\" field") );

        write_file( DBDIR + job.arguments()[0], job.content() );

        resp.status()->add_message("done");

        return WORKER_SUCCESS;
    }

    Worker::response_t
    WorkerTestBasic::do_post_testbasic_thing_v1( const Job & job, JobResponse & resp ) {
        resp.status()->progress(10);
        resp.status()->add_message("processing");
        
        if( job.operation() == "create" ) {
            // post-create where the resource id is created for user
            // (instead of a PUT where the user specifies the name)
            
            // get the generated id
            Json in = job.json_content();
            in["id"] = job.resource_name();
            
            write_file( DBDIR + job.resource_name(), in.serialize() );
        }
        else { 
            // post update
            if( bfs::exists(DBDIR + job.arguments()[0]) ) {
                // update resource, our thing only has 1 updatable
                // field so just set it.
                const Json & in = job.json_content();
                if( in.hasKey("stuff") ) { 
                    Json out;
                    out.parseFile(DBDIR + job.arguments()[0]);
                    out["stuff"] = in["stuff"];
                    write_file(DBDIR + job.arguments()[0], out.serialize());
                }
            }
            else {
                gbTHROW( ERR_NOT_FOUND("thing \"" + job.arguments()[0] + "\" not found") );
            }
        }

        resp.status()->add_message("done");
        return WORKER_SUCCESS;
    }

    Worker::response_t
    WorkerTestBasic::do_delete_testbasic_thing_v1( const Job & job, JobResponse & resp ) {
        resp.status()->progress(10);
        resp.status()->add_message("processing");

        // don't actually delete if fake-out header is set
        if( job.headers().has_key("fake-out")
            && boost::lexical_cast<int>(job.headers()["fake-out"]) ) {
            resp.status()->add_message("ignoring delete due to fake-out header");
            return WORKER_SUCCESS;
        }
        
        if( job.arguments().empty() ) {
            gbTHROW( ERR_BAD_REQUEST("missing required resource name") );
        }

        std::string file = DBDIR + job.arguments()[0];
        if( bfs::exists(file) && bfs::is_regular_file(file) ) {
            // nuke the file from our db
            unlink( file.c_str() );
        }
        else {
            gbTHROW( ERR_NOT_FOUND("thing \"" + job.arguments()[0] + "\" not found") );
        }
        
        resp.status()->add_message("done");
        return WORKER_SUCCESS;
    }
}
