--------------
-- Триггеры --
--------------

BEGIN;

------------------
-- Конфигурация --
------------------

-- Нельзя добавлять новые настройки в конфигурацию.
DROP TRIGGER IF EXISTS configuration_insert;
CREATE TRIGGER configuration_insert BEFORE INSERT ON configuration
BEGIN
    SELECT RAISE(ROLLBACK, 'No new configuration settings are allowed.');
END;

-- Нельзя удалять настройки.
DROP TRIGGER IF EXISTS configuration_delete;
CREATE TRIGGER configuration_delete BEFORE DELETE ON configuration
BEGIN
    SELECT RAISE(ROLLBACK, 'Configuration settings may not be deleted.');
END;

-- Нельзя переименовывать настройки.
DROP TRIGGER IF EXISTS configuration_update_name;
CREATE TRIGGER configuration_update_name BEFORE UPDATE ON configuration
    WHEN NEW.name IS NOT NULL AND NEW.name != OLD.name
BEGIN
    SELECT RAISE(ROLLBACK, 'Settings may not be renamed.');
END;

-- Нельзя менять заголовки настроек.
DROP TRIGGER IF EXISTS configuration_update_title;
CREATE TRIGGER configuration_update_title BEFORE UPDATE ON configuration
    WHEN NEW.title IS NOT NULL AND NEW.title != OLD.title
BEGIN
    SELECT RAISE(ROLLBACK, 'Setting titles may not be changed.');
END;

-- Настройки только для чтения не могут быть изменены.
DROP TRIGGER IF EXISTS configuration_update_read_only;
CREATE TRIGGER configuration_update_read_only BEFORE UPDATE ON configuration
    WHEN OLD.read_only = 1
BEGIN
    SELECT RAISE(ROLLBACK, 'Read only settings may not be changed.');
END;


-------------------------------------------
-- Идентификаторы не могут быть изменены --
-------------------------------------------

-- Пользователь
DROP TRIGGER IF EXISTS user_update_id;
CREATE TRIGGER user_update_id BEFORE UPDATE ON user
    WHEN NEW.id IS NOT NULL AND NEW.id != OLD.id
BEGIN
    SELECT RAISE(ROLLBACK, 'User ID may not be changed.');
END;

-- Сообщение
DROP TRIGGER IF EXISTS post_update_id;
CREATE TRIGGER post_update_id BEFORE UPDATE ON post
    WHEN NEW.id IS NOT NULL AND NEW.id != OLD.id
BEGIN
    SELECT RAISE(ROLLBACK, 'Post ID may not be changed.');
END;

-- Реакция
DROP TRIGGER IF EXISTS reaction_update_id;
CREATE TRIGGER reaction_update_id BEFORE UPDATE ON reaction
    WHEN NEW.id IS NOT NULL AND NEW.id != OLD.id
BEGIN
    SELECT RAISE(ROLLBACK, 'Reaction ID may not be changed.');
END;

COMMIT;
