BEGIN;

-- Глобальные настройки.

INSERT INTO configuration (name, value, title, read_only)
    VALUES ('"uuid.regexp"',
            '"[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}"',
            '"Regular expression to validate UUID"', 1);


COMMIT;
