%%
%% %CopyrightBegin%
%%
%% Copyright Ericsson AB 1997-2021. All Rights Reserved.
%%
%% Licensed under the Apache License, Version 2.0 (the "License");
%% you may not use this file except in compliance with the License.
%% You may obtain a copy of the License at
%%
%%     http://www.apache.org/licenses/LICENSE-2.0
%%
%% Unless required by applicable law or agreed to in writing, software
%% distributed under the License is distributed on an "AS IS" BASIS,
%% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%% See the License for the specific language governing permissions and
%% limitations under the License.
%%
%% %CopyrightEnd%
%%
%%

-module(httpd).

-behaviour(inets_service).

-include("httpd_internal.hrl").

%% Behavior callbacks
-export([
	 start_standalone/1, 
	 start_service/1, 
	 stop_service/1, 
	 services/0, 
	 service_info/1
	]).

%% API
-export([
         parse_query/1,
         reload_config/2,
         info/1,
         info/2,
         info/3,
         info/4
        ]).

-deprecated({parse_query, 1,
            "use uri_string:dissect_query/1 instead"}).

%%%========================================================================
%%% API
%%%========================================================================

-spec parse_query(QueryString) -> QueryList | uri_string:error() when
      QueryString :: string(),
      QueryList :: [{unicode:chardata(), unicode:chardata() | true}].
parse_query(String) ->
    uri_string:dissect_query(String).

-spec reload_config(Config, Mode) -> ok | {error, Reason} | no_return() when
      Config :: file:name_all() | [{Option, Value}],
      Mode   :: non_disturbing | disturbing | blocked,
      Option :: atom(),
      Value  :: term(),
      Reason :: term().
reload_config(Config = [Value| _], Mode) when is_tuple(Value) ->
    do_reload_config(Config, Mode);
reload_config(ConfigFile, Mode) ->
    try file:consult(ConfigFile) of
        {ok, [PropList]} ->
            %% Erlang terms format
            do_reload_config(PropList, Mode)
    catch
        exit:_ ->
            throw({error, {could_not_consult_proplist_file, ConfigFile}})
    end.

-spec info(Pid) -> HttpInformation when
      Pid :: pid(),
      Path :: file:name_all(),
      HttpInformation :: [CommonOption]
                       | [CommunicationOption]
                       | [ModOption]
                       | [LimitOption]
                       | [AdminOption],
      CommonOption :: {port, non_neg_integer()}
                | {server_name, string()}
                | {server_root, Path}
                | {document_root, Path},
      CommunicationOption :: {bind_address, inet:ip_address() | inet:hostname() | any}
        | {profile, atom()}
        | { socket_type,
            ip_comm | {ip_comm, ssl:tls_option() | gen_tcp:option()} | {ssl, ssl:tls_option() | gen_tcp:option()}}
        | {ipfamily, inet | inet6}
        | {minimum_bytes_per_second, integer()},
      ModOption :: {modules, atom()},
      LimitOption :: {customize, atom()}
                   | {disable_chunked_transfer_encoding_send, boolean()}
                   | {keep_alive, boolean()}
                   | {keep_alive_timeout, integer()}
                   | {max_body_size, integer()}
                   | {max_clients, integer()}
                   | {max_header_size, integer()}
                   | {max_content_length, integer()}
                   | {max_uri_size, integer()}
                   | {max_keep_alive_request, integer()}
                   | {max_client_body_chunk, integer()},
      AdminOption :: {mime_types, [{MimeType :: string(), Extension :: string()}] | Path}
                   | {mime_type, string()}
                   | {server_admin, string()}
                   | {server_tokens, none|prod|major|minor|minimal|os|full|{private, string()}}
                   | {logger, Options::list()}
                   | {log_format, common | combined}
                   | {error_log_format, pretty | compact}.
info(Pid) when is_pid(Pid) ->
    info(Pid, []).

-spec info(Pid, HttpInformation) -> HttpInformation  when
      Pid     :: pid(),
      Path :: file:name_all(),
      HttpInformation :: [CommonOption]
                       | [CommunicationOption]
                       | [ModOption]
                       | [LimitOption]
                       | [AdminOption],
      CommonOption :: {port, non_neg_integer()}
                | {server_name, string()}
                | {server_root, Path}
                | {document_root, Path},
      CommunicationOption :: {bind_address, inet:ip_address() | inet:hostname() | any}
        | {profile, atom()}
        | { socket_type,
            ip_comm | {ip_comm, ssl:tls_option() | gen_tcp:option()} | {ssl, ssl:tls_option() | gen_tcp:option()}}
        | {ipfamily, inet | inet6}
        | {minimum_bytes_per_second, integer()},
      ModOption :: {modules, atom()},
      LimitOption :: {customize, atom()}
                   | {disable_chunked_transfer_encoding_send, boolean()}
                   | {keep_alive, boolean()}
                   | {keep_alive_timeout, integer()}
                   | {max_body_size, integer()}
                   | {max_clients, integer()}
                   | {max_header_size, integer()}
                   | {max_content_length, integer()}
                   | {max_uri_size, integer()}
                   | {max_keep_alive_request, integer()}
                   | {max_client_body_chunk, integer()},
      AdminOption :: {mime_types, [{MimeType :: string(), Extension :: string()}] | Path}
                   | {mime_type, string()}
                   | {server_admin, string()}
                   | {server_tokens, none|prod|major|minor|minimal|os|full|{private, string()}}
                   | {logger, Options::list()}
                   | {log_format, common | combined}
                   | {error_log_format, pretty | compact};
          (Address, Port) -> HttpInformation when
      Address :: inet:ip_address(),
      Port    :: integer(),
      Path :: file:name_all(),
      HttpInformation :: [CommonOption]
                       | [CommunicationOption]
                       | [ModOption]
                       | [LimitOption]
                       | [AdminOption],
      CommonOption :: {port, non_neg_integer()}
                | {server_name, string()}
                | {server_root, Path}
                | {document_root, Path},
      CommunicationOption :: {bind_address, inet:ip_address() | inet:hostname() | any}
        | {profile, atom()}
        | { socket_type,
            ip_comm | {ip_comm, ssl:tls_option() | gen_tcp:option()} | {ssl, ssl:tls_option() | gen_tcp:option()}}
        | {ipfamily, inet | inet6}
        | {minimum_bytes_per_second, integer()},
      ModOption :: {modules, atom()},
      LimitOption :: {customize, atom()}
                   | {disable_chunked_transfer_encoding_send, boolean()}
                   | {keep_alive, boolean()}
                   | {keep_alive_timeout, integer()}
                   | {max_body_size, integer()}
                   | {max_clients, integer()}
                   | {max_header_size, integer()}
                   | {max_content_length, integer()}
                   | {max_uri_size, integer()}
                   | {max_keep_alive_request, integer()}
                   | {max_client_body_chunk, integer()},
      AdminOption :: {mime_types, [{MimeType :: string(), Extension :: string()}] | Path}
                   | {mime_type, string()}
                   | {server_admin, string()}
                   | {server_tokens, none|prod|major|minor|minimal|os|full|{private, string()}}
                   | {logger, Options::list()}
                   | {log_format, common | combined}
                   | {error_log_format, pretty | compact}.
info(Pid, Properties) when is_pid(Pid) andalso is_list(Properties) ->
    {ok, ServiceInfo} = service_info(Pid), 
    Address = proplists:get_value(bind_address, ServiceInfo),
    Port = proplists:get_value(port, ServiceInfo),
    Profile = proplists:get_value(profile, ServiceInfo, default),
    case Properties of
	[] ->
	    info(Address, Port, Profile);
	_ ->
	    info(Address, Port, Profile, Properties)
    end; 

info(Address, Port) when is_integer(Port) ->
    info(Address, Port, default).

-spec info(Address, Port, Profile) -> HttpInformation when
      Address :: inet:ip_address() | any,
      Port    :: integer(),
      Profile :: atom(),
      Path :: file:name_all(),
      HttpInformation :: [CommonOption]
                       | [CommunicationOption]
                       | [ModOption]
                       | [LimitOption]
                       | [AdminOption],
      CommonOption :: {port, non_neg_integer()}
                | {server_name, string()}
                | {server_root, Path}
                | {document_root, Path},
      CommunicationOption :: {bind_address, inet:ip_address() | inet:hostname() | any}
        | {profile, atom()}
        | { socket_type,
            ip_comm | {ip_comm, ssl:tls_option() | gen_tcp:option()} | {ssl, ssl:tls_option() | gen_tcp:option()}}
        | {ipfamily, inet | inet6}
        | {minimum_bytes_per_second, integer()},
      ModOption :: {modules, atom()},
      LimitOption :: {customize, atom()}
                   | {disable_chunked_transfer_encoding_send, boolean()}
                   | {keep_alive, boolean()}
                   | {keep_alive_timeout, integer()}
                   | {max_body_size, integer()}
                   | {max_clients, integer()}
                   | {max_header_size, integer()}
                   | {max_content_length, integer()}
                   | {max_uri_size, integer()}
                   | {max_keep_alive_request, integer()}
                   | {max_client_body_chunk, integer()},
      AdminOption :: {mime_types, [{MimeType :: string(), Extension :: string()}] | Path}
                   | {mime_type, string()}
                   | {server_admin, string()}
                   | {server_tokens, none|prod|major|minor|minimal|os|full|{private, string()}}
                   | {logger, Options::list()}
                   | {log_format, common | combined}
                   | {error_log_format, pretty | compact};
          (Address, Port, Properties) -> HttpInformation when
      Address :: inet:ip_address() | any,
      Port    :: integer(),
      Properties :: [atom()],
      Path :: file:name_all(),
      HttpInformation :: [CommonOption]
                       | [CommunicationOption]
                       | [ModOption]
                       | [LimitOption]
                       | [AdminOption],
      CommonOption :: {port, non_neg_integer()}
                | {server_name, string()}
                | {server_root, Path}
                | {document_root, Path},
      CommunicationOption :: {bind_address, inet:ip_address() | inet:hostname() | any}
        | {profile, atom()}
        | { socket_type,
            ip_comm | {ip_comm, ssl:tls_option() | gen_tcp:option()} | {ssl, ssl:tls_option() | gen_tcp:option()}}
        | {ipfamily, inet | inet6}
        | {minimum_bytes_per_second, integer()},
      ModOption :: {modules, atom()},
      LimitOption :: {customize, atom()}
                   | {disable_chunked_transfer_encoding_send, boolean()}
                   | {keep_alive, boolean()}
                   | {keep_alive_timeout, integer()}
                   | {max_body_size, integer()}
                   | {max_clients, integer()}
                   | {max_header_size, integer()}
                   | {max_content_length, integer()}
                   | {max_uri_size, integer()}
                   | {max_keep_alive_request, integer()}
                   | {max_client_body_chunk, integer()},
      AdminOption :: {mime_types, [{MimeType :: string(), Extension :: string()}] | Path}
                   | {mime_type, string()}
                   | {server_admin, string()}
                   | {server_tokens, none|prod|major|minor|minimal|os|full|{private, string()}}
                   | {logger, Options::list()}
                   | {log_format, common | combined}
                   | {error_log_format, pretty | compact}.
info(Address, Port, Profile) when is_integer(Port), is_atom(Profile) ->
    httpd_conf:get_config(Address, Port, Profile);

info(Address, Port, Properties) when is_integer(Port) andalso 
				     is_list(Properties) ->    
    httpd_conf:get_config(Address, Port, default, Properties).

-spec info(Address, Port, Profile, Properties) -> HttpInformation when
      Address :: inet:ip_address() | any,
      Port    :: integer(),
      Profile :: atom(),
      Properties :: [atom()],
      Path :: file:name_all(),
      HttpInformation :: [CommonOption]
                       | [CommunicationOption]
                       | [ModOption]
                       | [LimitOption]
                       | [AdminOption],
      CommonOption :: {port, non_neg_integer()}
                | {server_name, string()}
                | {server_root, Path}
                | {document_root, Path},
      CommunicationOption :: {bind_address, inet:ip_address() | inet:hostname() | any}
        | {profile, atom()}
        | { socket_type,
            ip_comm | {ip_comm, ssl:tls_option() | gen_tcp:option()} | {ssl, ssl:tls_option() | gen_tcp:option()}}
        | {ipfamily, inet | inet6}
        | {minimum_bytes_per_second, integer()},
      ModOption :: {modules, atom()},
      LimitOption :: {customize, atom()}
                   | {disable_chunked_transfer_encoding_send, boolean()}
                   | {keep_alive, boolean()}
                   | {keep_alive_timeout, integer()}
                   | {max_body_size, integer()}
                   | {max_clients, integer()}
                   | {max_header_size, integer()}
                   | {max_content_length, integer()}
                   | {max_uri_size, integer()}
                   | {max_keep_alive_request, integer()}
                   | {max_client_body_chunk, integer()},
      AdminOption :: {mime_types, [{MimeType :: string(), Extension :: string()}] | Path}
                   | {mime_type, string()}
                   | {server_admin, string()}
                   | {server_tokens, none|prod|major|minor|minimal|os|full|{private, string()}}
                   | {logger, Options::list()}
                   | {log_format, common | combined}
                   | {error_log_format, pretty | compact}.
info(Address, Port, Profile, Properties) when is_integer(Port) andalso
					      is_atom(Profile) andalso is_list(Properties) ->    
    httpd_conf:get_config(Address, Port, Profile, Properties).


%%%========================================================================
%%% Behavior callbacks
%%%========================================================================

start_standalone(Config0) ->
    Config = httpd_ssl_wrapper(Config0),
    httpd_sup:start_link([{httpd, Config}], stand_alone).

start_service(Config0) ->
    Config = httpd_ssl_wrapper(Config0),
    httpd_sup:start_child(Config).

httpd_ssl_wrapper(Config0) ->
    case proplists:get_value(socket_type, Config0) of
        {essl, Value} ->
            lists:keyreplace(socket_type, 1, Config0, {socket_type, {ssl, Value}});
        {ssl, Value} ->
            lists:keyreplace(socket_type, 1, Config0, {socket_type, {essl, Value}});
        _ -> Config0
    end.


stop_service({Address, Port}) ->
    stop_service({Address, Port, ?DEFAULT_PROFILE});
stop_service({Address, Port, Profile}) ->
    Name  = httpd_util:make_name("httpd_instance_sup", Address, Port, Profile),
    Pid = whereis(Name),
    MonitorRef = erlang:monitor(process, Pid),
    Result = httpd_sup:stop_child(Address, Port, Profile),
    receive
        {'DOWN', MonitorRef, _, _, _} ->
            Result
    end;     
stop_service(Pid) when is_pid(Pid) ->
    case service_info(Pid)  of
	{ok, Info} ->	   
	    Address = proplists:get_value(bind_address, Info),
	    Port = proplists:get_value(port, Info),
	    Profile = proplists:get_value(profile, Info, ?DEFAULT_PROFILE),
	    stop_service({Address, Port, Profile});
	Error ->
	    Error
    end.
	    
services() ->
    [{httpd, ChildPid} || {_, ChildPid, _, _} <- 
			      supervisor:which_children(httpd_sup)].
service_info(Pid) ->
    try
	[{ChildName, ChildPid} || 
	    {ChildName, ChildPid, _, _} <- 
		supervisor:which_children(httpd_sup)] of
	Children ->
	    child_name2info(child_name(Pid, Children))
    catch
	exit:{noproc, _} ->
	    {error, service_not_available} 
    end.

%%%--------------------------------------------------------------
%%% Internal functions
%%%--------------------------------------------------------------------

child_name(_, []) ->
    undefined;
child_name(Pid, [{Name, Pid} | _]) ->
    Name;
child_name(Pid, [_ | Children]) ->
    child_name(Pid, Children).

-spec child_name2info(undefined | HTTPSup) -> Object when
      HTTPSup :: {httpd_instance_sup, any, Port, Profile}
               | {httpd_instance_sup, Address, Port, Profile},
      Port    :: integer(),
      Address :: inet:ip_address() | any,
      Profile :: atom(),
      Object  :: {error, no_such_service} | {ok, [tuple()]}.
child_name2info(undefined) ->
    {error, no_such_service};
child_name2info({httpd_instance_sup, any, Port, Profile}) ->
    {ok, Host} = inet:gethostname(),
    Info = info(any, Port, Profile, [server_name]),
    {ok, [{bind_address,  any}, {host, Host}, {port, Port} | Info]};
child_name2info({httpd_instance_sup, Address, Port, Profile}) ->
    Info = info(Address, Port, Profile, [server_name]),
    case inet:gethostbyaddr(Address) of
	{ok, {_, Host, _, _,_, _}} ->
	    {ok, [{bind_address, Address}, 
		  {host, Host}, {port, Port} | Info]};
	_  ->
	    {ok, [{bind_address, Address}, {port, Port} | Info]}
    end.


reload(Config, Address, Port, Profile) ->
    Name = make_name(Address,Port, Profile),
    case whereis(Name) of
	Pid when is_pid(Pid) ->
	    httpd_manager:reload(Pid, Config);
	_ ->
	    {error,not_started}
    end.

    
%%% =========================================================
%%% Function:    block/3, block/4
%%%              block(Addr, Port, Mode)
%%%              block(ConfigFile, Mode, Timeout)
%%%              block(Addr, Port, Mode, Timeout)
%%% 
%%% Returns:     ok | {error,Reason}
%%%              
%%% Description: This function is used to block an HTTP server.
%%%              The blocking can be done in two ways, 
%%%              disturbing or non-disturbing. Default is disturbing.
%%%              When a HTTP server is blocked, all requests are rejected
%%%              (status code 503).
%%% 
%%%              disturbing:
%%%              By performing a disturbing block, the server
%%%              is blocked forcefully and all ongoing requests
%%%              are terminated. No new connections are accepted.
%%%              If a timeout time is given then, on-going requests
%%%              are given this much time to complete before the
%%%              server is forcefully blocked. In this case no new 
%%%              connections is accepted.
%%% 
%%%              non-disturbing:
%%%              A non-disturbing block is more graceful. No
%%%              new connections are accepted, but the ongoing 
%%%              requests are allowed to complete.
%%%              If a timeout time is given, it waits this long before
%%%              giving up (the block operation is aborted and the 
%%%              server state is once more not-blocked).
%%%
%%% Types:       Port       -> integer()             
%%%              Addr       -> {A,B,C,D} | string() | undefined
%%%              ConfigFile -> string()
%%%              Mode       -> disturbing | non_disturbing
%%%              Timeout    -> integer()
%%%

block(Addr, Port, Profile, disturbing) when is_integer(Port) ->
    do_block(Addr, Port, Profile, disturbing);
block(Addr, Port, Profile, non_disturbing) when is_integer(Port) ->
    do_block(Addr, Port, Profile, non_disturbing).
do_block(Addr, Port, Profile, Mode) when is_integer(Port) andalso is_atom(Mode) -> 
    Name = make_name(Addr, Port, Profile),
    case whereis(Name) of
	Pid when is_pid(Pid) ->
	    httpd_manager:block(Pid, Mode);
	_ ->
	    {error,not_started}
    end.
    
%%% =========================================================
%%% Function:    unblock/2
%%%              unblock(Addr, Port)
%%%              
%%% Description: This function is used to reverse a previous block 
%%%              operation on the HTTP server.
%%%
%%% Types:       Port       -> integer()             
%%%              Addr       -> {A,B,C,D} | string() | undefined
%%%              ConfigFile -> string()
%%%

unblock(Addr, Port, Profile) when is_integer(Port) -> 
    Name = make_name(Addr,Port, Profile),
    case whereis(Name) of
	Pid when is_pid(Pid) ->
	    httpd_manager:unblock(Pid);
	_ ->
	    {error,not_started}
    end.


make_name(Addr, Port, Profile) ->
    httpd_util:make_name("httpd", Addr, Port, Profile).


do_reload_config(ConfigList, Mode) ->
    case (catch httpd_conf:validate_properties(ConfigList)) of
	{ok, Config} ->
	    Address = proplists:get_value(bind_address, Config, any), 
	    Port    = proplists:get_value(port, Config, 80),
	    Profile = proplists:get_value(profile, Config, default),
	    case block(Address, Port, Profile, Mode) of
		ok ->
		    reload(Config, Address, Port, Profile),
		    unblock(Address, Port, Profile);
		Error ->
		    Error
	    end;
	Error ->
	    Error
    end.

%%%--------------------------------------------------------------
%%% Deprecated 
%%%--------------------------------------------------------------
