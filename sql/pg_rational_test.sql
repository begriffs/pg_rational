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

-- from float
select 0.263157894737::float::rational;
select 3.141592625359::float::rational;
select 0.606557377049::float::rational;
select -0.5::float::rational;

-- to float
select '1/2'::rational::float;
select '1/3'::rational::float;
select '-1/2'::rational::float;

-- too big
select '2147483648/2147483647'::rational;
-- no spaces
select '1 /3'::rational;
-- no zero denominator
select '1/0'::rational;
-- quoted number treated as int
select '1'::rational;
select '-1'::rational;
-- no garbage
select ''::rational;
select '/'::rational;
select '2/'::rational;
select '/2'::rational;
select 'sdfkjsdfj34984538'::rational;

-- simplification

-- double negative becomes positive
select rational_simplify('-1/-3');
-- works with negative value
select rational_simplify('-3/12');
-- dodge the INT32_MIN/-1 mistake
select rational_simplify('-2147483648/2147483647');
-- don't move negative if it would overflow
select rational_simplify('1/-2147483648');
-- biggest value reduces
select rational_simplify('2147483647/2147483647');
-- smallest value reduces
select rational_simplify('-2147483648/-2147483648');
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
select '2147483647/2147483647'::rational + '1/1';
-- overflow (sqrt(max)+1)/1 + 1/sqrt(max)
select '46342/1'::rational + '1/46341';

-- multiplication

-- multiplicative identity
select '1/1'::rational * '1/2';
-- multiplicative inverse
select '2/1'::rational * '1/2';
-- just regular
select '5/8'::rational * '3/5';
-- forcing intermediate simplification
select '2147483647/2147483647'::rational * '2/2';
-- overflow
select '46342/46341'::rational * '46341/46342';

-- division

select 1::rational / 3;
select '2/3'::rational / '2/3';

-- negation

-- flips sign of numerator
select -('1/2'::rational);
-- flips back
select -('-1/2'::rational);
-- overflow not possible
select -('-2147483648/1'::rational);
select -('1/-2147483648'::rational);
select -('-2147483648/-2147483648'::rational);

-- subtraction

-- just regular
select '1/2'::rational - '1/2';
-- can go negative
select '1/2'::rational - '1/1';
-- forcing intermediate simplification
select '2147483647/2147483647'::rational - '100/100';
-- overflow (sqrt(max)+1)/1 - 1/sqrt(max)
select '46342/1'::rational - '1/46341';

-- comparison

-- equal in every way
select '1/1'::rational = '1/1';
-- same equivalence class
select '20/40'::rational = '22/44';
-- negatives work too
select '-20/40'::rational = '-22/44';
-- overflow not possible
select '46342/46341'::rational = '46342/46341';
-- high precision
select '1/2147483647'::rational = '1/2147483646';
select '1/3'::rational * 3 = 1;
select 1.0/3.0 = 1.0;
-- not everything is equal
select '2/3'::rational = '8/5';

-- negates equality
select '1/1'::rational <> '1/1';
-- overflow not possible
select '46342/46341'::rational <> '46342/46341';
-- not equal
select '2/3'::rational <> '8/5';

-- lt anti-reflexive
select '1/2'::rational < '1/2';
-- gt anti-reflexive
select '1/2'::rational > '1/2';
-- overflow not possible
select '1/2147483647'::rational < '2/2147483647';

-- lte
select r
  from unnest(ARRAY[
      '303700050/303700050',
      '-2/1',
      '0/9999999',
      '-11/17',
      '100/1',
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
      '303700050/303700050',
      '-2/1',
      '0/9999999',
      '-11/17',
      '100/1',
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

-- aggregates

select min(r)
  from unnest(ARRAY[
      '100/1',
      NULL,
      '-11/17',
      '-1/1'
    ]::rational[]) as r;

select max(r)
  from unnest(ARRAY[
      '100/1',
      NULL,
      '-11/17',
      '-1/1'
    ]::rational[]) as r;

select max(r)
  from unnest(ARRAY[
      NULL, NULL, NULL
    ]::rational[]) as r;

select rational_simplify(sum(r))
  from unnest(ARRAY[
      '1/1',  '1/2', NULL,
      '-3/2', '1/16'
    ]::rational[]) as r;

select sum(r)
  from unnest(ARRAY[
      NULL, NULL, NULL
    ]::rational[]) as r;

-- stern-brocot intermediates

-- intermediates start at 1 -- between 0 and Infinity
select rational_intermediate(NULL, NULL);
-- random example
select rational_intermediate('15/16', 1);
select rational_intermediate('15/16', 1)
  between '15/16'::rational and 1;
select rational_intermediate('44320/39365', '77200/12184');
select rational_intermediate('44320/39365', '77200/12184')
  between '44320/39365'::rational and '77200/12184';
-- unbounded upper limit produces least greater integer
select rational_intermediate('1/3', NULL);
select rational_intermediate('3/2', NULL);
-- though not the other direction
select rational_intermediate(NULL, '15/16');
