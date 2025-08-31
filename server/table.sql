CREATE DATABASE dongRuanSystem;

USE dongRuanSystem

CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    role ENUM('admin', 'factory', 'expert', 'auditor') NOT NULL,
    email VARCHAR(100),
    company VARCHAR(100),
    approved BOOLEAN DEFAULT FALSE,
    status ENUM('pending', 'approved', 'rejected') DEFAULT 'pending',
    reviewed_by INT, 
    FOREIGN KEY (reviewed_by) REFERENCES users(id)
);

CREATE TABLE user_tokens (
    token VARCHAR(64) PRIMARY KEY,
    user_id INT NOT NULL,
    role ENUM('admin', 'factory', 'expert', 'auditor') NOT NULL,
    expire_time DATETIME NOT NULL,
    approved BOOL DEFAULT FALSE,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

