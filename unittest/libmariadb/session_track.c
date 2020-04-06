#include "my_test.h"

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

struct my_tests_st my_tests[] = {
  {"test_system_variables", test_system_variables, TEST_CONNECTION_NEW, 0,  NULL,  NULL},
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
