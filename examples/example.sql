-- MiniDB Example Queries
-- Run these after starting minidb to try it out

-- Create a students table
CREATE TABLE students (id INT, name TEXT, age INT);

-- Insert some data
INSERT INTO students VALUES (1, 'Sri Ram', 20);
INSERT INTO students VALUES (2, 'Arun', 19);
INSERT INTO students VALUES (3, 'Priya', 21);
INSERT INTO students VALUES (4, 'Karthik', 20);
INSERT INTO students VALUES (5, 'Meera', 22);

-- Query all students
SELECT * FROM students;

-- Query with conditions
SELECT * FROM students WHERE age > 20;
SELECT * FROM students WHERE age = 20;
SELECT name, age FROM students WHERE id = 1;

-- Multiple conditions
SELECT * FROM students WHERE age >= 20 AND age <= 21;

-- Delete a student
DELETE FROM students WHERE id = 3;

-- Verify deletion
SELECT * FROM students;

-- Meta commands
.tables
.schema
