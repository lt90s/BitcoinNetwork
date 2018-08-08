import pymysql

from config import MYSQL

DB = pymysql.connect(host=MYSQL['host'], user=MYSQL['user'], password=MYSQL['password'],
                     db=MYSQL['db'], charset=MYSQL['charset'], cursorclass=pymysql.cursors.DictCursor)

ADDR_TABLE_SQL = 'create table if not exists bitcoin_address ('\
                        'id int primary key auto_increment,'\
                        'ip varchar(64) unique,'\
                        'port int,'\
                        'agent varchar(128),'\
                        'version int,'\
                        'services int,'\
                        'country varchar(32),'\
                        'region varchar(32),'\
                        'city varchar(32))'

INSERT_ADDR_TABLE_IP_SQL = "insert into `bitcoin_address` (`ip`, `port`, `agent`, `version`, `services`, "\
                        "`country`, `region`, `city`) values(%s, %s, '', 0, 0, '', '', '')"

UPDATE_ADDR_TABLE_GEO_SQL = "update `bitcoin_address` set `country`=%s, `region`=%s, `city`=%s where `ip`=%s"

UPDATE_ADDR_TABLE_VERSION_SQL = "update `bitcoin_address` set `agent`=%s, `version`=%s, `services`=%s where `ip`=%s"

QUERY_COUNTRY_DISTRIBUTION_SQL = "select country, count(*) as count from bitcoin_address group by country order by count desc"

QUERY_REGION_DISTRIBUTION_SQL = "select region, count(*) as count from bitcoin_address where country=%s group by region order by count desc"


def init_db():
    with DB.cursor() as cursor:
        cursor.execute(ADDR_TABLE_SQL)