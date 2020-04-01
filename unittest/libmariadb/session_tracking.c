/*
Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

The MySQL Connector/C is licensed under the terms of the GPLv2
<http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
MySQL Connectors. There are special exceptions to the terms and
conditions of the GPLv2 as it is applied to this software, see the
FLOSS License Exception
<http://www.mysql.com/about/legal/licensing/foss-exception.html>.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include "my_test.h"
#include "ma_common.h"

#include <mysql/client_plugin.h>

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

static int test_session_track_with_proctor(MYSQL *mysql)
{
  // Skip if mariadb or if the plugin is not installed

  int rc;
  MYSQL_RES *result;
  my_bool sestrac_exists = FALSE;

  rc = mysql_query(mysql, "drop table if exists t1");
  check_mysql_rc(rc, mysql);
  check_must_have_session_tracking(mysql);

  rc = mysql_query(mysql, "create table t1 (c1 INT) Engine=InnoDB");
  check_mysql_rc(rc, mysql);
  check_must_have_session_tracking(mysql);

  rc = mysql_query(mysql, "insert into t1 values (1)");
  check_mysql_rc(rc, mysql);
  check_must_have_session_tracking(mysql);

  diag("select * from t1");
  rc = mysql_query(mysql, "select * from t1");
  check_mysql_rc(rc, mysql);
  result = mysql_store_result(mysql);
  check_must_have_session_tracking(mysql);

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
  {"test_session_track_with_proctor", test_session_track_with_proctor, TEST_CONNECTION_DEFAULT, 0, NULL, NULL},
  {NULL, NULL, 0, 0, NULL, 0}
};


int main(int argc, char **argv)
{
  if (argc > 1)
    get_options(argc, argv);

  get_envvars();

  run_tests(my_tests);

  return(exit_status());
}
