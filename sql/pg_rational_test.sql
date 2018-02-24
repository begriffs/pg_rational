create extension pg_rational;
set client_min_messages to error;

-- I/O

-- can parse a simple fraction
select '1/3'::rational;
-- can parse negatives
select '-1/3'::rational;
select '1/-3'::rational;
-- SEND works
select rational_send('1/3');

-- casting

-- tuple helper
select (1,2)::ratt = '1/2'::rational;
-- int
select 42 = '42/1'::rational;
-- bigint
select 42::bigint = '42/1'::rational;

-- too big
select '9223372036854775808/9223372036854775807'::rational;
-- no spaces
select '1 /3'::rational;
-- no zero denominator
select '1/0'::rational;
-- no single numbers
select '1'::rational;
-- no garbage
select ''::rational;
select 'sdfkjsdfj34984538'::rational;

-- simplification

-- double negative becomes positive
select rational_simplify('-1/-3');
-- works with negative value
select rational_simplify('-3/12');
-- dodge the INT64_MIN/-1 mistake
select rational_simplify('-9223372036854775808/9223372036854775807');
-- don't move negative if it would overflow
select rational_simplify('1/-9223372036854775808');
-- biggest value reduces
select rational_simplify('9223372036854775807/9223372036854775807');
-- smallest value reduces
select rational_simplify('-9223372036854775808/-9223372036854775808');
-- idempotent on simplified expression
select rational_simplify('1/1');

-- addition

-- additive identity
select '0/1'::rational + '1/2';
-- additive inverse
select '1/2'::rational + '-1/2';
-- just regular
select '1/2'::rational + '1/2';
-- forcing intermediate simplification
select '9223372036854775807/9223372036854775807'::rational + '1/1';
-- overflow (sqrt(max)+1)/1 + 1/sqrt(max)
select '3037000501/1'::rational + '1/3037000500';

-- multiplication

-- multiplicative identity
select '1/1'::rational * '1/2';
-- multiplicative inverse
select '2/1'::rational * '1/2';
-- just regular
select '5/8'::rational * '3/5';
-- forcing intermediate simplification
select '9223372036854775807/9223372036854775807'::rational * '2/2';
-- overflow
select '3037000501/3037000500'::rational * '3037000501/3037000500';

-- division

select 1::rational / 3;
select '2/3'::rational / '2/3';

-- negation

-- flips sign of numerator
select -('1/2'::rational);
-- flips back
select -('-1/2'::rational);
-- overflow not possible
select -('-9223372036854775808/1'::rational);
select -('1/-9223372036854775808'::rational);
select -('-9223372036854775808/-9223372036854775808'::rational);

-- subtraction

-- just regular
select '1/2'::rational - '1/2';
-- can go negative
select '1/2'::rational - '1/1';
-- forcing intermediate simplification
select '9223372036854775807/9223372036854775807'::rational - '100/100';
-- overflow (sqrt(max)+1)/1 - 1/sqrt(max)
select '3037000501/1'::rational - '1/3037000500';

-- comparison

-- equal in every way
select '1/1'::rational = '1/1';
-- same equivalence class
select '20/40'::rational = '22/44';
-- negatives work too
select '-20/40'::rational = '-22/44';
-- overflow not possible
select '3037000501/3037000500'::rational = '3037000501/3037000500';
-- high precision
select '1/9223372036854775807'::rational = '1/9223372036854775806';
select (1.0::double precision)/9223372036854775807 = 1.0/9223372036854775806;
-- not everything is equal
select '2/3'::rational = '8/5';

-- negates equality
select '1/1'::rational <> '1/1';
-- overflow not possible
select '3037000501/3037000500'::rational <> '3037000501/3037000500';
-- not equal
select '2/3'::rational <> '8/5';

-- lt anti-reflexive
select '1/2'::rational < '1/2';
-- gt anti-reflexive
select '1/2'::rational > '1/2';
-- overflow not possible
select '1/9223372036854775807'::rational < '2/9223372036854775807';

-- lte
select r
  from unnest(ARRAY[
      '3037000501/3037000501',
      '-2/1',
      '0/9999999',
      '-11/17',
      '100/1',
      '-9223372036854775808/9223372036854775807',
      '3/4',
      '-1/2',
      '-1/1',
      '5/8',
      '6/9',
      '5/8'
    ]::rational[]) as r
order by r asc;
-- gte
select r
  from unnest(ARRAY[
      '3037000501/3037000501',
      '-2/1',
      '0/9999999',
      '-11/17',
      '100/1',
      '-9223372036854775808/9223372036854775807',
      '3/4',
      '-1/2',
      '-1/1',
      '5/8',
      '6/9',
      '5/8'
    ]::rational[]) as r
order by r desc;

-- btree
create table rs (
  r rational
);
create index rs_r_btree on rs using btree(r);
insert into rs values ('0/7'), ('1/7'), ('2/7'), ('3/7'),
                      ('4/7'), ('5/7'), ('6/7');
set enable_seqscan=false;

explain select * from rs where r > '1/7' and r <= '10/14';
select * from rs where r > '1/7' and r <= '10/14';

set enable_seqscan=true;
drop table rs cascade;

-- hash
create table rs (
  r rational
);
create index rs_r_hash on rs using hash(r);
insert into rs values ('0/7'), ('1/7');
set enable_seqscan=false;

explain select * from rs where r = '0/1';
select * from rs where r = '0/1';
select * from rs where r = '2/7';

set enable_seqscan=true;
drop table rs cascade;

-- stern-brocot intermediates

-- random example
select rational_intermediate('15/16', 1);
select rational_intermediate('15/16', 1)
  between '15/16'::rational and 1;
select rational_intermediate('44320/39365', '77200/12184');
select rational_intermediate('44320/39365', '77200/12184')
  between '44320/39365'::rational and '77200/12184';
-- cutting it closer
select rational_intermediate('72650000/72659999', 1);
select rational_intermediate('72650000/72659999', 1)
  between '72650000/72659999'::rational and 1;
-- unbounded upper limit produces least greater integer
select rational_intermediate('1/3', NULL);
select rational_intermediate('3/2', NULL);
-- though not the other direction
select rational_intermediate(NULL, '15/16');
