use test_db;

insert into test_proxima_be_restart values(7, 'name7', 17.111, 700, '[1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,7]');

select count(*) from test_proxima_be_restart;
