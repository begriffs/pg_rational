create extension pg_rational;

-- input

-- can parse a simple fraction
select '1/3'::rational;
-- interprets negative numerator
select '-1/3'::rational;
-- moves negative value from denom to numer
select '1/-3'::rational;
-- double negative becomes positive
select '-1/-3'::rational;
-- biggest values
select '9223372036854775807/9223372036854775807'::rational;

-- SEND works
select rational_send('1/3');

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
