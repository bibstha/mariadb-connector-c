#include "my_test.h"

static my_bool has_session_tracking(MYSQL *mysql);
static void display_session_tracking(MYSQL *mysql);

#define check_no_session_tracking(m) \
if (has_session_tracking(m)) \
{ \
  diag("Session tracking found in %s line %d", __FILE__, __LINE__); \
  return FAIL; \
}

#define check_must_have_session_tracking(m) \
if (!has_session_tracking(m)) \
{ \
  diag("Session tracking not found in %s line %d", __FILE__, __LINE__); \
  return FAIL; \
} \
else \
{ \
  display_session_tracking(m); \
}

static int test_system_variables(MYSQL *mysql)
{
  int rc;
  const char *data;
  size_t len;

  if (!(mysql->server_capabilities & CLIENT_SESSION_TRACKING))
  {
    diag("Server doesn't support session tracking (cap=%lu)", mysql->server_capabilities);
    return SKIP;
  }

  rc= mysql_query(mysql, "USE mysql");
  check_mysql_rc(rc, mysql);
  FAIL_IF(strcmp(mysql->db, "mysql"), "Expected new schema 'mysql'");

  FAIL_IF(mysql_session_track_get_first(mysql, SESSION_TRACK_SCHEMA, &data, &len),
      "session_track_get_first failed");
  FAIL_IF(strncmp(data, "mysql", len), "Expected new schema 'mysql'");

  rc= mysql_query(mysql, "USE test");
  check_mysql_rc(rc, mysql);
  FAIL_IF(strcmp(mysql->db, "test"), "Expected new schema 'test'");

  FAIL_IF(mysql_session_track_get_first(mysql, SESSION_TRACK_SCHEMA, &data, &len),
      "session_track_get_first failed");
  FAIL_IF(strncmp(data, "test", len), "Expected new schema 'test'");

  diag("charset: %s", mysql->charset->csname);
  rc= mysql_query(mysql, "SET NAMES utf8");
  check_mysql_rc(rc, mysql);
  if (!mysql_session_track_get_first(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len))
    do {
      printf("# SESSION_TRACK_VARIABLES: %*.*s\n", (int)len, (int)len, data);
    } while (!mysql_session_track_get_next(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len));
  diag("charset: %s", mysql->charset->csname);
  FAIL_IF(strcmp(mysql->charset->csname, "utf8"), "Expected charset 'utf8'");

  rc= mysql_query(mysql, "SET NAMES latin1");
  check_mysql_rc(rc, mysql);
  FAIL_IF(strcmp(mysql->charset->csname, "latin1"), "Expected charset 'latin1'");

  rc= mysql_query(mysql, "DROP PROCEDURE IF EXISTS p1");
  check_mysql_rc(rc, mysql);

  rc= mysql_query(mysql, "CREATE PROCEDURE p1() "
      "BEGIN "
      "SET @@autocommit=0; "
      "SET NAMES utf8; "
      "SET session auto_increment_increment=2; "
      "END ");
  check_mysql_rc(rc, mysql);

  rc= mysql_query(mysql, "CALL p1()");
  check_mysql_rc(rc, mysql);

  if (!mysql_session_track_get_first(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len))
    do {
      printf("# SESSION_TRACK_VARIABLES: %*.*s\n", (int)len, (int)len, data);
    } while (!mysql_session_track_get_next(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len));

  rc= mysql_query(mysql, "SET @@SESSION.session_track_state_change=ON;");
  check_mysql_rc(rc, mysql);

  rc= mysql_query(mysql, "USE mysql;");
  check_mysql_rc(rc, mysql);

  FAIL_IF(mysql_session_track_get_first(mysql, SESSION_TRACK_SCHEMA, &data, &len),
      "Expected to find SESSION_TRACK_SCHEMA");
  FAIL_IF(strncmp(data, "mysql", len), "Expected to find 'mysql'");

  FAIL_IF(mysql_session_track_get_first(mysql, SESSION_TRACK_STATE_CHANGE, &data, &len),
      "Expected to find SESSION_TRACK_STATE_CHANGE");
  FAIL_IF(strncmp(data, "1", len), "Expected to find '1'");

  return OK;
}

static int test_session_track_with_proctor(MYSQL *mysql)
{
  int rc;

  rc = mysql_query(mysql, "SELECT EXISTS"
      "(SELECT * FROM INFORMATION_SCHEMA.PLUGINS where PLUGIN_NAME='MYSQL_PROCTOR')");
  check_mysql_rc(rc, mysql);
  MYSQL_RES *result = mysql_store_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(result);
  if (row[0][0] == '0')
  {
    diag("MYSQL_PROCTOR plugin not found");
    return SKIP;
  }

  rc = mysql_query(mysql, "drop table if exists t1");
  check_mysql_rc(rc, mysql);
  check_must_have_session_tracking(mysql);

  rc = mysql_query(mysql, "create table t1 (c1 INT) Engine=InnoDB");
  check_mysql_rc(rc, mysql);
  check_must_have_session_tracking(mysql);

  rc = mysql_query(mysql, "insert into t1 values (1)");
  check_mysql_rc(rc, mysql);
  check_must_have_session_tracking(mysql);

  const char *data;
  size_t len;

  FAIL_IF(mysql_session_track_get_first(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len),
      "Expected to find SESSION_TRACK_SYSTEM_VARIABLES");
  FAIL_IF(strncmp("mysql_proctor_innodb_performance_data", data, len),
      "Expected to find 'mysql_proctor_innodb_performance_data'");
  mysql_session_track_get_next(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len);
  FAIL_IF(len < 1, "Expected proctor value to be greater than 1 in length");

  // make a backup of data 
  char *data1;
  size_t len1;
  data1 = (char*)malloc(len + 1);
  memcpy(data1, data, len);
  data1[len] = 0;
  len1 = len;


  diag("select * from t1");
  rc = mysql_query(mysql, "select * from t1");
  check_mysql_rc(rc, mysql);
  mysql_store_result(mysql);
  check_must_have_session_tracking(mysql);

  FAIL_IF(mysql_session_track_get_first(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len),
      "Expected to find SESSION_TRACK_SYSTEM_VARIABLES");
  FAIL_IF(strncmp("mysql_proctor_innodb_performance_data", data, len),
      "Expected to find 'mysql_proctor_innodb_performance_data'");
  mysql_session_track_get_next(mysql, SESSION_TRACK_SYSTEM_VARIABLES, &data, &len);
  FAIL_IF(len < 1, "Expected proctor value to be greater than 1 in length");

  FAIL_IF(!strncmp(data, data1, MIN(len, len1)), "Expected proctor values to be different for the last two queries");

  free(data1);

  diag("drop table if exists t1");
  rc = mysql_query(mysql, "drop table if exists t1");
  check_mysql_rc(rc, mysql);
  check_must_have_session_tracking(mysql);

  return OK;
}

static my_bool has_session_tracking(MYSQL *mysql)
{
  my_bool found = FALSE;
  enum enum_session_state_type type;
  for (type = SESSION_TRACK_BEGIN; type <= SESSION_TRACK_END; type++)
  {
    const char *data;
    size_t length;

    if (mysql_session_track_get_first(mysql, type, &data, &length) == 0)
    {
      found = TRUE;
      break;
    }
  }
  return found;
}

static void display_session_tracking(MYSQL *mysql)
{
  enum enum_session_state_type type;
  for (type = SESSION_TRACK_BEGIN; type <= SESSION_TRACK_END; type++)
  {
    const char *data;
    size_t length;

    if (mysql_session_track_get_first(mysql, type, &data, &length) == 0)
    {
      /* print info type and initial data */
      printf("Type=%d:\n", type);
      printf("mysql_session_track_get_first(): length=%d; data=%*.*s\n",
          (int) length, (int) length, (int) length, data);

      /* check for more data */
      while (mysql_session_track_get_next(mysql, type, &data, &length) == 0)
      {
        printf("mysql_session_track_get_next(): length=%d; data=%*.*s\n",
            (int) length, (int) length, (int) length, data);
      }
    }
  }
}

struct my_tests_st my_tests[] = {
  {"test_system_variables", test_system_variables, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
  {"test_session_track_with_proctor", test_session_track_with_proctor, TEST_CONNECTION_DEFAULT, 0, NULL, NULL},
  {NULL, NULL, 0, 0, NULL, NULL}
};

int main(int argc, char **argv)
{
  if (argc > 1)
    get_options(argc, argv);

  get_envvars();

  run_tests(my_tests);

  return(exit_status());
}
