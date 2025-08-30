CREATE DATABASE dongRuanSystem;

USE dongRuanSystem

CREATE TABLE users{
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    role ENUM('admin', 'factory', 'expert', 'auditor') NOT NULL
};

INSERT INTO users(username, password,role) 
VALUES
    ('factory1', 'woxihuanxian123', 'factory'),
    ('expert1', 'beijingtairele123', 'expert'),
    ('admin1', 'iamadmin123', 'admin');