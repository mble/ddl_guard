SET bytea_output TO escape;
SELECT lo_create(42);
\lo_unlink 42

CREATE TABLE lotest_stash_values (loid oid, fd integer);
INSERT INTO lotest_stash_values (loid) SELECT lo_creat(42);

-- lobs require xacts
BEGIN;
UPDATE lotest_stash_values SET fd = lo_open(loid, CAST(x'20000' | x'40000' AS integer));
SELECT lowrite(fd, '
My hair is grey, but not with years,
Nor grew it white
    In a single night,
As men''s have grown from sudden fears:
My limbs are bowed, though not with toil,
  But rusted with a vile repose,
For they have been a dungeon''s spoil,
  And mine has been the fate of those
To whom the goodly earth and air
Are banned, and barred--forbidden fare;
But this was for my father''s faith
I suffered chains and courted death;
That father perished at the stake
For tenets he would not forsake;
And for the same his lineal race
In darkness found a dwelling place;
We were seven--who now are one,
  Six in youth, and one in age,
Finished as they had begun,
  Proud of Persecution''s rage;
One in fire, and two in field,
Their belief with blood have sealed,
Dying as their father died,
For the God their foes denied;--
Three were in a dungeon cast,
Of whom this wreck is left the last.
') FROM lotest_stash_values;

SELECT lo_close(fd) FROM lotest_stash_values;
END;

SELECT lo_from_bytea(0, lo_get(loid)) AS newloid FROM lotest_stash_values
\gset

BEGIN;
UPDATE lotest_stash_values SET fd=lo_open(loid, CAST(x'20000' | x'40000' AS integer));
SELECT lo_truncate(fd, 11) FROM lotest_stash_values;
SELECT lo_close(fd) FROM lotest_stash_values;
END;

BEGIN;
UPDATE lotest_stash_values SET fd = lo_open(loid, CAST(x'20000' | x'40000' AS integer));
SELECT lo_lseek64(fd, 4294967296, 0) FROM lotest_stash_values;
SELECT lowrite(fd, 'offset:4GB') FROM lotest_stash_values;
SELECT lo_truncate64(fd, 5000000000) FROM lotest_stash_values;
SELECT lo_close(fd) FROM lotest_stash_values;
END;

SELECT lo_unlink(loid) from lotest_stash_values;

TRUNCATE lotest_stash_values;

\set filename 'test/data/test.data'
\lo_import :filename

-- cleanup
SELECT lo_unlink(oid) FROM pg_largeobject_metadata;
DROP TABLE lotest_stash_values;