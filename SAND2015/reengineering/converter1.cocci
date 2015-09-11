virtual use_api
virtual force
virtual specific_locks
virtual debug_mode
virtual structure_info

@initialize:ocaml@

let cfile = ref "" (* set in header.cocci *)

let safe_int_of_string str =
  try int_of_string str
  with _ -> failwith ("bad int_of_string "^str)

type rd = READ | WRITE | READWRITE
let read_write_table =
  (Hashtbl.create(101) :
     ((string * string * string),(string list ref * rd ref)) Hashtbl.t)
let structure_output = ref ""

let all_mutexes =
  ref (None : ((string (*file*) * int (*line*)) * int (*num*)) list option)

let make_init_path = function
    "" -> "./"
  | s ->
      let len = String.length s in
      if String.get s (len-1) = '/'
      then String.sub s 0 (len-1)
      else s^"/"

let upcase s =
  String.uppercase
    (String.concat "_"
      (Str.split (Str.regexp "-")
        (String.concat "_" (Str.split (Str.regexp_string ".") s))))

let parse_profile i init_path header_file project =
  let init_path = make_init_path init_path in
  let info = ref [] in
  let partial_path = ref ("",None) in
  let partial_mutex = ref [] in
  let mutexes = ref [] in
  let stats = ref [] in
  let start = Str.regexp_string "==================================" in
  let init = Str.regexp_string "pthread_mutex_init" in
  let rec find_start _ =
    let l = input_line i in
    if Str.string_match start l 0
    then
      (let path = input_line i in
      let l = input_line i in
      if Str.string_match start l 0
      then
	begin
	  (match Str.split (Str.regexp " : ") path with
	    [path;aux] -> partial_path := (path,Some aux)
	  | [path] -> partial_path := (path,None)
	  | _ -> failwith ("unexpected path format: "^path));
	  mutex_info false
	end
      else failwith "expected double row of =s")
    else find_start()
  and mutex_info seen_mutexes =
    let l = input_line i in
    match Str.split (Str.regexp " ") l with
      "Mutex"::rest::_ ->
	let mutex_number =
	  List.hd
	    (Str.split (Str.regexp "#")
	       (List.hd (Str.split (Str.regexp " ") rest))) in
	mutexes := mutex_number::!mutexes;
	let call = input_line i in
	(match Str.split init call with
	  [before;after] ->
	    let real_call = input_line i in
	    (match Str.split (Str.regexp ":") real_call with
	      [before_and_file;line_and_after] ->
		(match Str.split (Str.regexp "\t") before_and_file with
		  [file] ->
		    (match Str.split (Str.regexp "\t") line_and_after with
		      line::_ ->
			partial_mutex :=
			  (mutex_number,
			   ((init_path ^ (fst !partial_path) ^ "/" ^ file),
			    (snd !partial_path)),
			   (safe_int_of_string line) - 1) :: // !!!!! -1
			  !partial_mutex;
			mutex_info true
		    | _ -> failwith "expected file and line")
		| _ -> failwith "expected file and line")
	    | _ -> failwith "expected file and line")
	| _ ->
	    Printf.fprintf stderr
	      "warning: mutex %s in %s not created with init\n"
	      mutex_number (fst !partial_path);
	    mutex_info seen_mutexes)
    | "mutrace:"::rest::_ when seen_mutexes ->
	let _blank = input_line i in
	let _legend = input_line i in
	mutex_stats ()
    | _ -> mutex_info seen_mutexes
  and mutex_stats _ =
    let l = input_line i in
    match Str.split (Str.regexp " +")  l with
      [mutex;locked;changed;cont;ttime;atime;mtime;_;flags] (*l2*)
      | [mutex;locked;changed;cont;ttime;atime;mtime;flags] ->
Printf.fprintf stderr "found someinformation\n";
	if List.mem mutex !mutexes
	then
	  begin
	    stats :=
              (mutex,safe_int_of_string locked,safe_int_of_string cont) ::
	      !stats;
	    mutex_stats()
	  end
	else restart()
    | _ -> Printf.printf "bad line %s\n" l; restart()
  and restart _ =
    let mutex_info =
      List.sort compare
	(List.map
	   (function (mutex_number, file, line) ->
	     let stats =
	       List.filter
		 (function (mutex_number1,locked,cont) ->
		   mutex_number = mutex_number1)
		 !stats in
	     match stats with
	       [stats] -> ((file,line),stats)
	     | _ -> failwith "no stats for mutex")
	   !partial_mutex) in
    let mutex_info =
      let rec loop = function
	  [] -> []
	| (location,mutex_and_stat)::xs ->
	    (match loop xs with
	      (location1,mutexes_and_stats)::rest when location = location1 ->
		(location1,mutex_and_stat::mutexes_and_stats)::rest
	    | rest -> (location,[mutex_and_stat]) :: rest) in
      loop mutex_info in
    let mutex_info =
      List.map
	(function (location,mutex_and_stat) ->
	  (location,
	   List.rev
	     (List.sort
		(function (mutex_number,locked,cont) ->
		  function (mutex_number_a,locked_a,cont_a) ->
		    compare cont cont_a)
		mutex_and_stat)))
	mutex_info in
    (if List.mem project (Str.split (Str.regexp "/") (fst !partial_path))
    then info := (!partial_path,mutex_info) :: !info);
    partial_path := ("",None);
    partial_mutex := [];
    mutexes := [];
    stats := [];
    find_start() in
  try find_start()
  with End_of_file ->
    (match (!partial_path,!mutexes,!stats) with
      (("",None),[],[]) ->
	let ctr = ref 0 in
	let o = open_out header_file in
	Printf.fprintf o "#ifndef LIBLOCK_CONFIG\n";
	Printf.fprintf o "#define LIBLOCK_CONFIG\n";
	Printf.fprintf o "#define TYPE_POSIX \"posix\"\n";
	Printf.fprintf o "#define DEFAULT_ARG NULL\n";
	Printf.fprintf o "#define TYPE_NOINFO TYPE_POSIX\n";
	Printf.fprintf o "#define ARG_NOINFO DEFAULT_ARG\n";
	Printf.fprintf o "#endif\n\n";
	let info = List.concat (snd (List.split !info)) in
	let info =
	  List.map
	    (function (((file,extra),line),mutex_and_stat) ->
	      ((file,line),(extra,mutex_and_stat)))
	    info in
	let info = List.sort compare info in
	let info =
	  let rec loop = function
	      [] -> []
	    | ((file,line),mutex_and_stat)::rest ->
		match loop rest with
		  ((filea,linea),mutex_and_stata)::resta
		  when file = filea && line = linea ->
		    ((filea,linea),mutex_and_stat::mutex_and_stata)::resta
		| resta -> ((file,line),[mutex_and_stat])::resta in
	  loop info in
	let info =
	  List.map
	    (function ((file,line),mutex_and_stats) ->
	      ctr := !ctr + 1;
	      Printf.fprintf o "// %s:%d\n" file line;
	      let print_rest mutex_and_stat =
		List.iter
		(function (mutex_number,locked,cont) ->
		  Printf.fprintf o
		    "// mutex %s \tlocked %d \tcont %d\n"
		    mutex_number locked cont)
		mutex_and_stat in
	      List.iter
		(function
		    (Some extra,mutex_and_stat) ->
		      Printf.fprintf o "// %s\n" extra;
		      print_rest mutex_and_stat
		  | (None,mutex_and_stat) ->
		      print_rest mutex_and_stat)
		mutex_and_stats;
	      let project = upcase project in
	      Printf.fprintf o
		"\n#define TYPE_%s_%d TYPE_POSIX\n" project !ctr;
	      Printf.fprintf o
		"#define ARG_%s_%d DEFAULT_ARG\n\n" project !ctr;
	      ((file,line),!ctr))
	    info in
	close_out o;
	all_mutexes := Some info;
	info
    | _ -> failwith "incomplete information")
// --------------------------------------------------------------------
// before any transformation

@getfile@
identifier virtual.pre_mutex_lock;
identifier virtual.pre_mutex_unlock;
position p;
@@

(
pre_mutex_lock@p(...)
|
pre_mutex_unlock@p(...)
|
pthread_create@p(...)
|
pthread_cond_wait@p(...)
|
pthread_cond_timedwait@p(...)
|
pthread_cond_signal@p(...)
|
pthread_cond_broadcast@p(...)
|
pthread_mutex_destroy@p(...)
)

@tgetfile@
typedef pthread_mutex_t;
@@

pthread_mutex_t

@script:ocaml@
p << getfile.p;
@@

    let fl = (List.hd p).file in
    if Filename.check_suffix fl ".c"
    then cfile := fl

// --------------------------------------------------------------------

@script:ocaml include_help depends on getfile || tgetfile@
full_project_header_file << virtual.full_project_header_file;
pinc;
@@
pinc := Printf.sprintf "#include <%s>\n//" full_project_header_file

@@
identifier include_help.pinc;
@@

#include <...>
+#include <liblock.h>
+pinc;
+#include <stdint.h>
// Generic renamings

@static_init depends on use_api && !specific_locks@
identifier the_lock;
@@

pthread_mutex_t the_lock = PTHREAD_MUTEX_INITIALIZER;

@script:ocaml static_init_fn@
the_lock << static_init.the_lock;
fn;
@@

fn :=
Printf.sprintf "%s;\nstatic __attribute__ ((constructor (105))) void init_%s() {\n  liblock_lock_init(TYPE_NOINFO, ARG_NOINFO, &%s, 0);\n}\n//"
the_lock the_lock the_lock

@@
identifier the_lock;
identifier static_init_fn.fn;
typedef liblock_lock_t;
@@

- pthread_mutex_t the_lock = PTHREAD_MUTEX_INITIALIZER;
+ liblock_lock_t fn;

// -----------------------------------------------------------------

@depends on use_api && !specific_locks@
@@

- pthread_mutex_t
+ liblock_lock_t

@depends on use_api@
@@

- pthread_create
+ liblock_thread_create
  (...)

@depends on use_api && !specific_locks@
@@

(
- pthread_cond_wait
+ liblock_cond_wait
|
- pthread_cond_timedwait
+ liblock_cond_timedwait
|
- pthread_cond_signal
+ liblock_cond_signal
|
- pthread_cond_broadcast
+ liblock_cond_broadcast
|
- pthread_mutex_destroy
+ liblock_lock_destroy
)
  (...)
@@
@@

- {
  pthread_mutex_lock(...);
- }

@@
@@

- {
  pthread_mutex_unlock(...);
- }

@@
@@

pthread_mutex_lock(...);
-;

@@
@@

pthread_mutex_unlock(...);
-;
@@
@@

struct {
  ...
  pthread_mutex_t mutex;
+ liblock_lock_t lmutex;
  ...
};

@pre_db@
typedef DB;
typedef DBC;
typedef FNAME;
expression a;
{DB *,FNAME *} b;
DBC * c;
@@

(
- MUTEX_LOCK(a,b->mutex)
+ BDMUTEX_LOCK(MLOCK(a,b->mutex))
|
- MUTEX_UNLOCK(a,b->mutex)
+ BDMUTEX_UNLOCK(MLOCK(a,b->mutex))
|
- MUTEX_LOCK(a,c->dbp->mutex)
+ BDMUTEX_LOCK(MLOCK(a,c->dbp->mutex))
|
- MUTEX_UNLOCK(a,c->dbp->mutex)
+ BDMUTEX_UNLOCK(MLOCK(a,c->dbp->mutex))
)

@depends on use_api && specific_locks@
{DB *,FNAME *} e;
DBC *f;
expression e1;
@@

- pthread_cond_wait
+ liblock_cond_wait
  (e1,&\(e\|f->dbp\)->mutex)

@depends on use_api && specific_locks@
{DB *,FNAME *} e;
DBC *f;
expression e1,e2;
@@
- pthread_cond_timedwait
+ liblock_cond_timedwait
  (e1,&\(e\|f->dbp\)->mutex,e2)

@depends on use_api && specific_locks@
{DB *,FNAME *} e;
DBC *f;
@@

(
- pthread_cond_signal
+ liblock_cond_signal
|
- pthread_cond_broadcast
+ liblock_cond_broadcast

)
  (&\(e\|f->dbp\)->mutex)

@depends on use_api && specific_locks@
{DB *,FNAME *} e;
DBC *f;
@@

- pthread_mutex_destroy
+ liblock_lock_destroy
  (&\(e\|f->dbp\)->mutex)

// ---------------------------------------------------------------------
// Some macros that introduce free variables or assignments

@@
expression f;
@@

(
- LF_CLR(f)
+               ((flags) &= ~(f))
|
- LF_ISSET(f)
+             ((flags) & (f))
|
- LF_SET(f)
+               ((flags) |= (f))
)

@@
expression x,e1,e2;
iterator name TAILQ_FOREACH;
statement S;
@@

TAILQ_FOREACH(x,e1,e2) S
+x=x;
@@
void *l;
identifier virtual.mutex_lock;
@@

- mutex_lock(l)
+ mutex_lock((liblock_lock_t *)(l))

@@
void *l;
identifier virtual.mutex_unlock;
@@

- mutex_unlock(l)
+ mutex_unlock((liblock_lock_t *)(l))

@@
typedef mr_lock_t;
mr_lock_t l;
@@

- liblock_lock_destroy(l)
+ liblock_lock_destroy((liblock_lock_t *)(l))
// ------------------------------------------------------------------------
// variable

@@
identifier virtual.pre_mutex_lock;
identifier virtual.the_lock;
@@

- pre_mutex_lock
+ liblock
  (...,&the_lock,...);

@@
identifier virtual.pre_mutex_unlock;
identifier virtual.the_lock;
@@

- pre_mutex_unlock
+ libunlock
  (...,&the_lock,...);

@depends on use_api@
identifier virtual.the_lock;
@@

- pthread_mutex_t
+ liblock_lock_t
  the_lock;

@depends on use_api@
identifier virtual.the_lock;
expression e;
@@

- pthread_cond_wait
+ liblock_cond_wait
  (e, &the_lock);

@depends on use_api@
identifier virtual.pre_mutex_init;
identifier virtual.the_lock;
expression e;
@@

- pre_mutex_init
+ libinit
  (&the_lock,e);

@depends on use_api@
identifier virtual.the_lock;
@@

(
- pthread_cond_signal
+ liblock_cond_signal
|
- pthread_cond_broadcast
+ liblock_cond_broadcast
)
  (&the_lock);

// ------------------------------------------------------------------------
// variable

@@
identifier virtual.pre_mutex_lock;
identifier virtual.the_second_lock;
@@

- pre_mutex_lock
+ liblock
  (...,&the_second_lock,...);

@@
identifier virtual.pre_mutex_unlock;
identifier virtual.the_second_lock;
@@

- pre_mutex_unlock
+ libunlock
  (...,&the_second_lock,...);

@depends on use_api@
identifier virtual.the_second_lock;
@@

- pthread_mutex_t
+ liblock_lock_t
  the_second_lock;

@depends on use_api@
identifier virtual.the_second_lock;
expression e;
@@

- pthread_cond_wait
+ liblock_cond_wait
  (e, &the_second_lock);

@depends on use_api@
identifier virtual.pre_mutex_init;
identifier virtual.the_second_lock;
expression e;
@@

- pre_mutex_init
+ libinit
  (&the_second_lock,e);


@depends on use_api@
identifier virtual.the_second_lock;
@@

(
- pthread_cond_signal
+ liblock_cond_signal
|
- pthread_cond_broadcast
+ liblock_cond_broadcast
)
  (&the_second_lock);

// ------------------------------------------------------------------------
// field

@@
identifier virtual.pre_mutex_lock;
expression p;
identifier virtual.the_field_lock;
@@

- pre_mutex_lock
+ liblock
  (&(p->the_field_lock));

@@
identifier virtual.pre_mutex_unlock;
expression p;
identifier virtual.the_field_lock;
@@

- pre_mutex_unlock
+ libunlock
  (...,&(p->the_field_lock),...);

@depends on use_api@
identifier I;
identifier virtual.the_field_lock;
@@

struct I { ...
- pthread_mutex_t
+ liblock_lock_t
  the_field_lock;
  ...
};

@depends on use_api@
expression p;
identifier virtual.the_field_lock;
expression e;
@@

- pthread_cond_wait
+ liblock_cond_wait
  (e, &(p->the_field_lock));

@depends on use_api@
identifier virtual.pre_mutex_init;
expression p;
identifier virtual.the_field_lock;
expression e;
@@

- pre_mutex_init
+ libinit
  (&(p->the_field_lock),e);

@depends on use_api@
expression p;
identifier virtual.the_field_lock;
@@

(
- pthread_cond_signal
+ liblock_cond_signal
|
- pthread_cond_broadcast
+ liblock_cond_broadcast
)
  (&(p->the_field_lock));

// ------------------------------------------------------------------------
// field

@@
identifier virtual.pre_mutex_lock;
expression p;
identifier virtual.the_second_field_lock;
@@

- pre_mutex_lock
+ liblock
  (...,&(p->the_second_field_lock),...);

@@
identifier virtual.pre_mutex_unlock;
expression p;
identifier virtual.the_second_field_lock;
@@

- pre_mutex_unlock
+ libunlock
  (...,&(p->the_second_field_lock),...);

@depends on use_api@
identifier I;
identifier virtual.the_second_field_lock;
@@

struct I { ...
- pthread_mutex_t
+ liblock_lock_t
  the_second_field_lock;
  ...
};

@depends on use_api@
expression p;
identifier virtual.the_second_field_lock;
expression e;
@@

- pthread_cond_wait
+ liblock_cond_wait
  (e, &(p->the_second_field_lock));

@depends on use_api@
identifier virtual.pre_mutex_init;
expression p;
identifier virtual.the_second_field_lock;
expression e;
@@

- pre_mutex_init
+ libinit
  (&(p->the_second_field_lock),e);

@depends on use_api@
expression p;
identifier virtual.the_second_field_lock;
@@

(
- pthread_cond_signal
+ liblock_cond_signal
|
- pthread_cond_broadcast
+ liblock_cond_broadcast
)
  (&(p->the_second_field_lock));

@initcall@
identifier virtual.mutex_init;
position p;
@@

mutex_init@p(...)

@script:ocaml make_names@
profile_file << virtual.profile_file;
header_file  << virtual.header_file;
init_path    << virtual.init_path;
project      << virtual.project;
p << initcall.p;
ty;
arg;
@@

let all_mutexes =
  match !all_mutexes with
    None ->
      let i = open_in profile_file in
      let res = parse_profile i init_path header_file project in
      close_in i;
      res
  | Some all_mutexes -> all_mutexes in
let project = upcase project in
let current_file = (List.hd p).file in
let current_line = (List.hd p).line in
try
  let ((file,line),number) =
    List.find
      (function ((file,line),numbernn) ->
	let file_len = String.length file in
	let pfile_len = String.length  current_file in
	let diff = pfile_len - file_len in
	if diff >= 0
	then (String.sub current_file diff file_len) = file &&
	  line = current_line
	else false)
      all_mutexes in
  ty  := Printf.sprintf "TYPE_%s_%d" project number;
  arg := Printf.sprintf "ARG_%s_%d"  project number
with Not_found ->
  Printf.fprintf stderr "no information about mutex in file %s on line %d\n"
    current_file current_line;
  ty  := "TYPE_NOINFO";
  arg := "ARG_NOINFO"

@@
identifier virtual.mutex_init;
position initcall.p;
expression list es;
identifier make_names.ty;
identifier make_names.arg;
@@

-mutex_init@p(es)
+liblock_lock_init(ty,arg,es)

// ---------------------------------------------------------------------
// ignore files where there is nothing else to do

@useful@
identifier virtual.mutex_lock;
position p;
@@

mutex_lock@p(...)

@script:ocaml depends on !useful@
ml << virtual.mutex_lock;
@@

Coccilib.exit()
// ifs - not safe, but looks like paranoia

@@
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
statement S;
expression x,l;
@@

(
- if ((x = mutex_lock(l)) != 0) S
+ mutex_lock(l);
|
- if ((x = mutex_unlock(l)) != 0) S
+ mutex_unlock(l);
|
- if ((mutex_lock(l)) != 0) S
+ mutex_lock(l);
|
- if ((mutex_unlock(l)) != 0) S
+ mutex_unlock(l);
)

@normalize_lock_arg@
expression e;
identifier fld;
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
@@

(
- mutex_lock(&e->fld);
+ mutex_lock(&(e->fld));
|
- mutex_unlock(&e->fld);
+ mutex_unlock(&(e->fld));
)

// find inner mutexes and functions that contain them
// got tired of trylock...

@@
expression e1;
@@

(
-  break;
+  __BREAK__;
|
-  return
+  __RETURN__
   ;
|
-  return
+  __RETURN__(
  e1
+ )
  ;
)

@exists@
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
expression e1,e3;
fresh identifier key = "";
statement S;
@@

if (...) { ... when != mutex_lock(e1);
               when any
 mutex_unlock(e1
+, key+1
 );
+{
... when != mutex_lock(e1);
(
  __BREAK__;
|
  __RETURN__;
|
  __RETURN__(e3);
|
  pthread_exit(e3);
|
  exit(e3);
)
+}
} else S

// do it again, to get else case
@exists@
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
expression e1,e3;
fresh identifier key = "";
statement S;
@@

if (...) { ... when != mutex_lock(e1);
               when any
 mutex_unlock(e1
+, key+1
 );
+{
... when != mutex_lock(e1);
(
  __BREAK__;
|
  __RETURN__;
|
  __RETURN__(e3);
|
  pthread_exit(e3);
|
  exit(e3);
)
+}
} else S

@marklocks exists@
expression e,key;
fresh identifier done = "done";
fresh identifier ret = "ret";
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
@@

mutex_lock(e
+, ret, done
 );
... when != mutex_unlock(e);
mutex_unlock(e,key);

// find a block with a unique start and end
@acceptable exists@
expression e;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
identifier ret,done;
identifier f;
statement S;
expression key;
@@

f(...) {
++ int ret;
... when any
mutex_lock(e
- , ret, done
 );
+ ret = 0;
<... when != mutex_unlock(e);
{
  ... 
+  ret = key;
+  __GOTO__(done);
-  mutex_unlock(e,key);
   S
}
 ...>
+ done: {}
mutex_unlock(e);
+ if (ret) { }
... when any
}

@exists@
identifier acceptable.done, acceptable.ret;
expression e;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
statement S;
expression key;
@@

mutex_lock(e)
... when != mutex_unlock(e)
    when any
  ret = key;
  __GOTO__(done);
- S
... when != mutex_unlock(e)
    when any
done: {}
mutex_unlock(e);
if (ret) {
++if (ret == key) S
...
}

@cleanup@
expression e;
@@

(
- __BREAK__;
+ break;
|
- __RETURN__;
+ return;
|
- __RETURN__ (
+ return
 (e)
- )
 ;
|
- __RETURN__
+ return
   (e);
)
// move declarations under locks and not in braces up over the top of the lock

@@
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
type T;
identifier x;
expression E,l;
@@

++T x;
mutex_lock(l);
... when != mutex_unlock(l)
    when any
    when != { ... }
(
-T x;
|
-T x 
+x
= E;
)
// pre check

// prevent later parsing problems due to a label with no following statement
@@
identifier virtual.mutex_unlock;
identifier l;
@@

l:
+{}
mutex_unlock(...);

@lock_without_unlock exists@
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
expression list e;
position p;
@@

mutex_lock@p(e)
... when != mutex_unlock(e)

@script:ocaml depends on !force@
e << lock_without_unlock.e;
p << lock_without_unlock.p;
@@

let line = (List.hd p).line in
let file = (List.hd p).file in
let file =
  if Str.string_match (Str.regexp_string "/tmp/") file 0
  then !cfile
  else file in
Printf.printf "lock on line %d in file %s has no unlock\n%s\n" line file e;
flush stdout;
Coccilib.exit()

@script:ocaml depends on force@
e << lock_without_unlock.e;
p << lock_without_unlock.p;
@@

let line = (List.hd p).line in
let file = (List.hd p).file in
let file =
  if Str.string_match (Str.regexp_string "/tmp/") file 0
  then !cfile
  else file in
Printf.printf "lock on line %d in file %s has no unlock\n%s\n" line file e;
flush stdout

@fix_lock_with_no_unlock@
identifier virtual.mutex_lock;
position lock_without_unlock.p;
expression e;
@@

- mutex_lock@p(e)
+ liblock_relock_in_cs(e)

@unlock_without_lock exists@
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
expression list e;
position p;
@@

... when != mutex_lock(e)
mutex_unlock@p(e)

@script:ocaml depends on !force@
p << unlock_without_lock.p;
@@

let line = (List.hd p).line in
let file = (List.hd p).file in
let file =
  if Str.string_match (Str.regexp_string "/tmp/") file 0
  then !cfile
  else file in
Printf.printf "unlock on line %d in file %s has no lock\n" line file;
flush stdout;
Coccilib.exit()

@script:ocaml depends on force@
p << unlock_without_lock.p;
@@

let line = (List.hd p).line in
let file = (List.hd p).file in
let file =
  if Str.string_match (Str.regexp_string "/tmp/") file 0
  then !cfile
  else file in
Printf.printf "unlock on line %d in file %s has no lock\n" line file;
flush stdout

@@
identifier virtual.mutex_unlock;
position unlock_without_lock.p;
expression e;
@@

- mutex_unlock@p(e)
+ liblock_unlock_in_cs(e)

// ------------------------------------------------------------------

// give each critical section its own name

@ml@
identifier virtual.mutex_lock;
position pml;
@@

mutex_lock@pml(...)

@mu@
identifier virtual.mutex_unlock;
position pmu;
@@

mutex_unlock@pmu(...)

// find a block with a unique start and end

@unique@
position p1,p2,ml.pml,mu.pmu;
expression e;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
identifier virtual.good1;
identifier virtual.good2;
@@

 mutex_lock@p1@pml(e)
<... when strict
    when != e
(
good1(...,e,...)
|
good2(...,e,...)
|
mutex_lock(e)
)
...>
mutex_unlock@p2@pmu(e)

@build@
fresh identifier fn = "function";
fresh identifier inst = "instance";
fresh identifier input = "input";
fresh identifier output = "output";
fresh identifier incontext = "incontext";
fresh identifier outcontext = "outcontext";
fresh identifier ctx = "ctx";
position unique.p1,unique.p2;
identifier virtual.mutex_unlock;
identifier mutex_lock;
expression lock;
@@

// in the following, the code incontext(outcontext(ctx)) just serves to
// save the metavariables 

+ {union inst {
+   struct input { int __nothing__; } input;
+   struct output { int __nothing__; } output; } inst = {{ 0, }};
  mutex_lock@p1(
- lock
+ incontext(outcontext(ctx))
 );
+{
...
+}
mutex_unlock@p2(
-lock
+ctx
 );
+ liblock_exec(lock,&fn,&inst);}

@script:ocaml debug depends on debug_mode@
inst << build.inst;
fn << build.fn;
current;
@@
current := Printf.sprintf "\"%s\"" fn

@depends on debug_mode@
expression lock;
identifier build.inst, build.fn, debug.current;
@@

(
+printf("started cs %s\n",current);
 union inst { ... } inst = ...;
|
 liblock_exec(lock,&fn,&inst);
+printf("ended cs %s\n",current);
)

// ------------------------------------------------------------------------

@transformedl@
identifier virtual.mutex_lock, build.inst;
position p;
@@

union inst { ... } inst = ...;
mutex_lock@p(...);

@oopsl exists@
identifier virtual.mutex_lock;
position p!=transformedl.p;
expression e;
identifier f;
@@

f(...) { <+...
mutex_lock@p(e);
...+> }

@script:ocaml depends on !force@
p << oopsl.p;
f << oopsl.f;
e << oopsl.e;
@@

let line = (List.hd p).line in
let file = (List.hd p).file in
let file =
  if Str.string_match (Str.regexp_string "/tmp/") file 0
  then !cfile
  else file in
Printf.printf "lock on line %d in function %s in file %s\n%s\n" line f file e;
flush stdout;
Coccilib.exit()

@script:ocaml depends on force@
p << oopsl.p;
f << oopsl.f;
@@

let line = (List.hd p).line in
let file = (List.hd p).file in
let file =
  if Str.string_match (Str.regexp_string "/tmp/") file 0
  then !cfile
  else file in
Printf.printf "lock on line %d in function %s in file %s\n" line f file;
flush stdout

@@
identifier virtual.mutex_lock;
position oopsl.p;
expression e;
@@

- mutex_lock@p(e)
+ liblock_relock_in_cs(e)

@transformedu@
identifier virtual.mutex_unlock;
position p;
@@

mutex_unlock@p(...);
liblock_exec(...);

@@
identifier virtual.mutex_lock;
position p != transformedu.p;
expression e;
@@

- mutex_unlock@p(e)
+ liblock_unlock_in_cs(e)
// manage variables that are live across critical section boundaries

@ides_in_cs exists@
identifier build.incontext,build.outcontext,build.ctx;// uniquely identify a CS
type T,T1;
local idexpression T i;
local idexpression T1[] i1;
position ip;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
@@

mutex_lock(incontext(outcontext(ctx) ) );
... when any
    when != mutex_unlock(ctx);
    when != mutex_lock(incontext(outcontext(ctx) ) );
(
i1@ip
|
i@ip
)
... when any
    when != mutex_unlock(ctx);
    when != mutex_lock(incontext(outcontext(ctx) ) );
mutex_unlock(ctx);

@id@
position ides_in_cs.ip;
identifier i;
@@

i@ip

@prevector exists@
position ip;
typedef vector;
typedef va_list;
identifier I;
idexpression {vector,va_list,struct I} i;
identifier virtual.mutex_lock;
@@

i
...
mutex_lock(...);
... when any
i@ip

@nonprevector exists@
position ip != prevector.ip;
idexpression {vector,va_list,struct I} i;
identifier virtual.mutex_lock;
@@

mutex_lock(...);
... when exists
    when any
i@ip

@pre_defined_before_and_used_inside exists@
identifier build.incontext,build.outcontext,build.ctx; // uniquely identify a CS
identifier id.i;
position ip != nonprevector.ip;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
expression E2,E3;
type T2;
statement S;
@@

// Here we assume that if there is no definition in the critical section,
// then there must be a definition before.  This solves the parameter case,
// as well as definitions by &.  Perhaps we should check for & within the
// lock as well, but it's not certain that that is a definition...

mutex_lock(incontext(outcontext(ctx) ) );
... when != i = E2
    when != T2 i;
    when != mutex_unlock(ctx);
    when != mutex_lock(incontext(outcontext(ctx) ) );
(
for (<+...i=E3...+>; ... ; ...) S
|
for (<+...i=E3...+>; ... ; ) S
|
for (<+...i=E3...+>; ; ... ) S
|
i = <+...i@ip...+>
|
 (<+...i@ip...+> && ...)
|
 (<+...i@ip...+> || ...)
|
i = E3
|
 i@ip
)
... when any
    when != mutex_unlock(ctx);
    when != mutex_lock(incontext(outcontext(ctx) ) );
mutex_unlock(ctx);

@defined_before_and_used_inside@
identifier id.i;
position pre_defined_before_and_used_inside.ip;
@@

i@ip

@pre_defined_inside_and_used_after exists@
identifier build.fn,build.inst; // uniquely identify a CS
identifier build.incontext, build.outcontext, build.ctx;
identifier id.i;
position ip;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
expression E,E1,E2,E3;
type T1,T2;
expression l;
@@

mutex_lock(incontext(outcontext(ctx) ) );
... when != mutex_unlock(ctx);
    when != mutex_lock(incontext(outcontext(ctx) ) );
    when any
 \(i = E\|i += E\|i -= E\|++i\|--i\|++i\|i--\)
... when != \(i = E1\|i += E1\|i -= E1\)
    when != T1 i;
mutex_unlock(ctx);
liblock_exec(l,&fn,&inst);
... when != i = E2
    when != T2 i;
(
i = <+...i@ip...+>
|
i = E3
|
i@ip
)

@defined_inside_and_used_after@
identifier id.i;
position pre_defined_inside_and_used_after.ip;
@@

i@ip

@undefined_inside_and_used_after depends on defined_inside_and_used_after
 exists@
identifier build.incontext, build.outcontext, build.ctx;
identifier id.i;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
expression E;
@@

mutex_lock(incontext(outcontext(ctx) ) );
++ i = i;
... when != i = E
mutex_unlock(ctx);
// Add the fields to the structure declaration
// Add the corresponding assignments

@depends on defined_inside_and_used_after disable add_signed, const_decl_init@
identifier build.inst,build.output;
type ides_in_cs.T;
identifier id.i;
@@

union inst { ...
   struct output {
     int __nothing__;
++   T i;
     ... } output;
 } inst = ...;

@tyd1@
identifier I;
type T1;
@@

typedef struct I { ... } T1;

@tyd2@ // clunky
identifier I;
type T2;
@@

typedef struct I T2;

@fix_flat_output1 disable const_decl_init@
identifier build.inst,build.output;
identifier id.i,I;
@@

union inst { ...
   struct output {
     ...
(
-    va_list i;
+    va_list *i;
|
-    struct I i;
+    struct I *i;
)
     ... } output;
 } inst = ...;

@fix_flat_output2 disable const_decl_init@
identifier build.inst,build.output;
identifier id.i;
type tyd1.T1;
@@

union inst { ...
   struct output {
     ...
-    T1 i;
+    T1 *i;
     ... } output;
 } inst = ...;

@fix_flat_output3 disable const_decl_init@
identifier build.inst,build.output;
identifier id.i;
type tyd2.T2;
@@

union inst { ...
   struct output {
     ...
-    T2 i;
+    T2 *i;
     ... } output;
 } inst = ...;

@depends on defined_inside_and_used_after disable add_signed, const_decl_init@
identifier build.inst,build.output;
type ides_in_cs.T1;
identifier id.i;
@@

union inst { ...
   struct output {
     int __nothing__;
++   T1 *i;
     ... } output;
 } inst = ...;

@addinput depends on defined_before_and_used_inside || undefined_inside_and_used_after
disable add_signed, const_decl_init@
identifier build.inst,build.input,build.incontext;
type ides_in_cs.T;
identifier id.i;
identifier virtual.mutex_lock;
expression e;
@@

union inst { 
  struct input {
    int __nothing__;
++  T i;
    ... } input; ... } inst = {{
0,
++ i,
...}};
mutex_lock(e);
++ T i = incontext->i;

// need three rules to avoid already tagged token, due to multiple orthogonal
// inherited values (tyds)
@fix_flat_input1 disable const_decl_init@
identifier build.inst,build.input;
identifier id.i,I;
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
identifier build.incontext,build.outcontext,build.ctx;
@@

union inst {
  struct input {
    ...
(
-    va_list i;
+    va_list *i;
|
-    struct I i;
+    struct I *i;
)
    ... } input; ... } inst = {{
...,
- i,
+ &i,
...}};
mutex_lock(incontext(outcontext(ctx) ) );
<...
(
- va_list i = incontext->i;
+ va_list *i = incontext->i;
|
-    struct I i = incontext->i;
+    struct I *i = incontext->i;
|
-  i
+ (*i)
)
...>
mutex_unlock(ctx);

@fix_flat_input2 disable const_decl_init@
identifier build.inst,build.input;
identifier id.i;
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
identifier build.incontext,build.outcontext,build.ctx;
type tyd1.T1;
@@

union inst {
  struct input {
    ...
-    T1 i;
+    T1 *i;
    ... } input; ... } inst = {{
...,
- i,
+ &i,
...}};
mutex_lock(incontext(outcontext(ctx) ) );
<...
(
-    T1 i = incontext->i;
+    T1 *i = incontext->i;
|
-  i
+ (*i)
)
...>
mutex_unlock(ctx);

@fix_flat_input3 disable const_decl_init@
identifier build.inst,build.input;
identifier id.i;
identifier virtual.mutex_lock;
identifier virtual.mutex_unlock;
identifier build.incontext,build.outcontext,build.ctx;
type tyd2.T2;
@@

union inst {
  struct input {
    ...
-    T2 i;
+    T2 *i;
    ... } input; ... } inst = {{
...,
- i,
+ &i,
...}};
mutex_lock(incontext(outcontext(ctx) ) );
<...
(
-    T2 i = incontext->i;
+    T2 *i = incontext->i;
|
-  i
+ (*i)
)
...>
mutex_unlock(ctx);

// Not sure how to do the T1 case (array).  Don't have the size.
@depends on !defined_before_and_used_inside &&
 !undefined_inside_and_used_after@
identifier build.incontext,build.outcontext,build.ctx;
type ides_in_cs.T;
identifier id.i;
identifier virtual.mutex_lock;
@@

mutex_lock(incontext(outcontext(ctx) ) );
++ T i;

@depends on defined_before_and_used_inside || undefined_inside_and_used_after
disable add_signed, const_decl_init@
identifier build.inst,build.input,build.incontext;
type ides_in_cs.T1;
identifier id.i;
identifier virtual.mutex_lock;
expression e;
@@

union inst { 
  struct input {
    int __nothing__;
++  T1 *i;
    ... } input; ... } inst = {{
0,
++ i,
...}};
mutex_lock(e);
++ T1 *i = incontext->i;

@depends on defined_inside_and_used_after@
identifier build.fn,build.inst,build.outcontext,build.output;
identifier id.i;
identifier virtual.mutex_unlock;
expression e,l;
@@

++ outcontext->i = i;
mutex_unlock(e);
liblock_exec(l,&fn,&inst);
++ i = inst.output.i;

// ------------------------------------------------------------------------

@last_output disable add_signed, const_decl_init@
identifier build.inst,build.output,build.fn,build.outcontext;
identifier virtual.mutex_unlock;
expression l,e;
type T;
identifier i;
typedef uintptr_t;
@@

(
 union inst {
   ...
   struct output { int __nothing__; } output; } inst = ...;
|
 union inst { // other big or unconvertable types?
   ...
   struct output { ... 
(
 MYREAL i;
|
 double i;
|
 float i;
|
 quad i;
)
 } output; } inst = ...;
|
 union inst {
   ...
   struct output { ...
- T i;
 } output; } inst = ...;
 ...
- outcontext->i = i;
...
+ __RETURN__((void *)(uintptr_t)i);
mutex_unlock(e);
+i = (T)(uintptr_t)(
 liblock_exec(l, &fn ,&inst)
+ )
;
...
- i = inst.output.i;
)

@one_input disable add_signed, const_decl_init@
identifier build.ctx,build.inst,build.input,build.output,build.fn;
identifier build.incontext;
identifier virtual.mutex_lock;
expression l,e,e1,v;
type T,T1,T2;
identifier i;
@@

(
 union inst { // other big or unconvertable types?
   struct input { int __nothing__;
(
 MYREAL i;
|
 double i;
|
 float i;
|
 quad i;
)
 } input; ... } inst = ...;
|
 union inst {
   struct input { int __nothing__; 
-  T i;
   } input;
   struct output { int __nothing__; } output; } inst
  = {{ 0,
- v,
  }};
mutex_lock(e);
...
-T i = incontext->i;
+T i = (T)(uintptr_t)ctx;
 ...
(
 liblock_exec(l, &fn, 
+v,
 &inst);
|
 e1 = (T1)(T2)(liblock_exec(l, &fn, 
+v,
 &inst));
)
)

@no_fields disable add_signed, const_decl_init@
identifier build.inst,build.input,build.output,build.fn;
expression l;
@@

- union inst {
-   struct input { int __nothing__; } input;
-   struct output { int __nothing__; } output; } inst = {{ 0, }};
 ...
(
liblock_exec(l,&fn,
- &inst
+ NULL
  )
|
liblock_exec(...)
)

@@
identifier inst,fn;
expression l,e;
@@

liblock_exec(l,&fn,e
- ,&inst
  )

// clean up, even ok for NULL
@fix_call_type@
expression e1,e2,e3;
@@

liblock_exec(e1,e3,
- e2
+ (void *)(uintptr_t)(e2)
 )

@no_inputs disable add_signed, const_decl_init@
identifier build.inst,build.input;
@@

  union inst {
-   struct input { int __nothing__; } input;
    ... } inst
-              = {{ 0, }}
    ;

@no_outputs disable add_signed, const_decl_init@
identifier build.inst,build.output;
@@

  union inst { ...
-   struct output { int __nothing__; } output;
    } inst = ...;

@disable add_signed, const_decl_init@
identifier build.inst,build.input;
@@

  union inst {
    struct input {
-                  int __nothing__;
                   ... } input;
    ... } inst
               = {{ 
-                 0,
                  ... }};

@disable add_signed@
identifier build.inst,build.output;
@@

  union inst { ...
    struct output {
-      int __nothing__;
       ... } output;
    } inst;

@depends on !no_inputs && !no_fields@
identifier build.inst, build.input, build.incontext, build.ctx;
identifier virtual.mutex_lock;
@@

  union inst { ... } inst;
  mutex_lock(...);
+ struct input *incontext = &(((union inst *)ctx)->input);

@depends on !no_outputs && !no_fields@
identifier build.inst, build.output, build.outcontext, build.ctx;
identifier virtual.mutex_lock;
@@

  union inst { ... } inst;
  mutex_lock(...);
+ struct output *outcontext = &(((union inst *)ctx)->output);

// ------------------------------------------------------------------------
// add alignment attributes

@@
identifier build.inst;
@@

union inst { ... }
+ __attribute__ ((aligned (CACHE_LINE_SIZE)))
inst;

@with_return@
position p;
identifier virtual.mutex_unlock;
@@

__RETURN__(...);
mutex_unlock@p(...);

@@
position p != with_return.p;
identifier virtual.mutex_unlock;
@@

+ __RETURN__(NULL);
mutex_unlock@p(...);

@exists@
identifier build.incontext, build.outcontext, build.ctx;
identifier virtual.mutex_unlock;
identifier mutex_lock;
@@

mutex_lock(incontext(outcontext(ctx) ) );
+{
...
+}
mutex_unlock(ctx);

@exists@
identifier build.inst;
type T;
identifier f;
field list fields;
initializer E;
@@

++ union inst { fields };
T f(...) {
... when any
(
- union inst { fields } inst = E;
+ union inst inst = E;
|
- union inst { fields } inst;
+ union inst inst;
)
... when any
}


@exists@
identifier build.fn, build.incontext, build.outcontext, build.ctx;
identifier virtual.mutex_unlock;
identifier virtual.mutex_lock;
statement S;
type T;
identifier f;
@@

++ void *fn(void *ctx);
++ void *fn(void *ctx) { S }
T f(...) {
... when any
mutex_lock(incontext(outcontext(ctx) ) );
S
-mutex_unlock(ctx);
... when any
}

@exists@
identifier build.incontext, build.outcontext, build.ctx;
identifier virtual.mutex_lock;
statement S;
@@

-mutex_lock(incontext(outcontext(ctx) ) );
+#if 0
S
+#endif

// cleanup return added for label case

@@
identifier l;
expression e;
@@

  l:
(
- __RETURN__ (e);
+ return e;
)

@@
expression e;
@@

(
- __RETURN__(e);
+ return e;
)

@@
identifier done;
@@

- __GOTO__(done);
+ goto done;

@pre_field_write depends on structure_info exists@
identifier build.fn, build.inst, build.input, build.output;
type T;
T e;
{union inst,struct input,struct output} e1;
identifier fld;
position p;
expression e2;
@@

fn(...) { <...
(
 e1.fld=e2
|
 e.fld@p=e2
) ...>
 }

@field_write exists@
position pre_field_write.p;
type T;
T e;
identifier fld;
identifier fn;
@@

fn(...) { ... when any
 e.fld@p ... when any
 }

@writelockname@
expression lock;
identifier field_write.fn;
expression e;
@@

liblock_exec(lock, &fn, e);

// ---------------------------------------------------------------------

@pre_field_readwrite depends on structure_info exists@
identifier build.fn, build.inst, build.input, build.output;
type T;
T e;
{union inst,struct input,struct output} e1;
identifier fld;
position p;
expression e2;
@@

fn(...) { <...
(
 e1.fld+=e2
|
 e1.fld-=e2
|
 e1.fld++
|
 e1.fld--
|
 e.fld@p+=e2
|
 e.fld@p-=e2
|
 e.fld@p++
|
 e.fld@p--
) ...>
 }

@field_readwrite exists@
position pre_field_readwrite.p;
type T;
T e;
identifier fld;
identifier fn;
@@

fn(...) { ... when any
 e.fld@p ... when any
 }

@readwritelockname@
expression lock;
identifier field_readwrite.fn;
expression e;
@@

liblock_exec(lock, &fn, e);

// ---------------------------------------------------------------------

@pre_field_read depends on structure_info exists@
identifier build.fn, build.inst, build.input, build.output;
type T;
T e;
{union inst,struct input,struct output} e1;
identifier fld;
position p != {pre_field_write.p,pre_field_readwrite.p};
@@

fn(...) { <...
(
 e1.fld
|
 e.fld@p
) ...>
 }

@field_read exists@
position pre_field_read.p;
type T;
T e;
identifier fld;
identifier fn;
@@

fn(...) { ... when any
 e.fld@p ... when any
 }

@readlockname@
expression lock;
identifier field_read.fn;
expression e;
@@

liblock_exec(lock, &fn, e)

// ---------------------------------------------------------------------

@script:ocaml@
t << field_write.T;
fld << field_write.fld;
f << field_write.fn;
lock << writelockname.lock;
out << virtual.structure_output;
@@

structure_output := out;
try
  let (functions,info) = Hashtbl.find read_write_table (lock,t,fld) in
  (match !info with
    READ -> info := READWRITE
  | WRITE -> ()
  | READWRITE -> () );
  if not (List.mem f !functions) then functions := f::!functions
with Not_found ->
  Hashtbl.add read_write_table (lock,t,fld) (ref [f],ref WRITE)

@script:ocaml@
t << field_readwrite.T;
fld << field_readwrite.fld;
f << field_readwrite.fn;
lock << readwritelockname.lock;
out << virtual.structure_output;
@@

structure_output := out;
try
  let (functions,info) = Hashtbl.find read_write_table (lock,t,fld) in
  (match !info with
    READ -> info := READWRITE
  | WRITE -> info := READWRITE
  | READWRITE -> () );
  if not (List.mem f !functions) then functions := f::!functions
with Not_found ->
  Hashtbl.add read_write_table (lock,t,fld) (ref [f],ref READWRITE)

@script:ocaml@
t << field_read.T;
fld << field_read.fld;
f << field_read.fn;
lock << readlockname.lock;
out << virtual.structure_output;
@@

structure_output := out;
try
  let (functions,info) = Hashtbl.find read_write_table (lock,t,fld) in
  (match !info with
    READ -> ()
  | WRITE -> info := READWRITE
  | READWRITE -> () );
  if not (List.mem f !functions) then functions := f::!functions
with Not_found ->
  Hashtbl.add read_write_table (lock,t,fld) (ref [f],ref READ)

@finalize:ocaml@

match !structure_output with
  "" -> ()
| out ->
  let o = open_out out in
  let l =
    Hashtbl.fold
      (function k -> function (fns,kd) -> function acc ->
  	(k,List.sort compare !fns,!kd) :: acc)
      read_write_table [] in
  let prev = ref "" in
  List.iter
    (function ((lock,t,fld),fns,kd) ->
      (if not (!prev = "") && not (lock = !prev)
      then
        begin
	  Printf.fprintf o "\n";
	  prev := lock
        end);
      Printf.fprintf o "%s: %s: %s.%s \t%s\n"
	(String.concat "" (Str.split (Str.regexp " ") lock) )
  	(match kd with READ -> " R" | WRITE -> " W" | READWRITE -> "RW")
	t fld
  	(String.concat " "
	  (List.map
	    (function f ->
              List.hd (Str.split (Str.regexp "function") f ) )
            fns) ) )
    (List.sort compare l);
  close_out o
@@
expression e1,e2;
@@

liblock_exec(
-MLOCK(
 e1,e2
-)
 ,...)