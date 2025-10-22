# MariaDB Setup Guide for Aquarium ML Service

## Overview

The Aquarium Water Change ML Predictor now uses **MariaDB exclusively** for data storage. This provides better performance, scalability, and remote access capabilities compared to SQLite.

## Prerequisites

- MariaDB or MySQL server (version 5.7+)
- Python 3.7 or higher
- MQTT broker (Mosquitto recommended)

## Installation Steps

### 1. Install MariaDB Server

#### Ubuntu/Debian:
```bash
sudo apt update
sudo apt install mariadb-server
sudo systemctl start mariadb
sudo systemctl enable mariadb
```

#### macOS (Homebrew):
```bash
brew install mariadb
brew services start mariadb
```

#### Verify Installation:
```bash
sudo systemctl status mariadb
# or
mysql --version
```

### 2. Secure MariaDB Installation

```bash
sudo mysql_secure_installation
```

Follow the prompts:
- Set root password: **Yes**
- Remove anonymous users: **Yes**
- Disallow root login remotely: **Yes**
- Remove test database: **Yes**
- Reload privilege tables: **Yes**

### 3. Create Database and User

```bash
sudo mysql -u root -p
```

In the MySQL shell:
```sql
-- Create database
CREATE DATABASE aquarium CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- Create user (replace 'your_password' with a strong password)
CREATE USER 'aquarium'@'localhost' IDENTIFIED BY 'your_password';

-- Grant privileges
GRANT ALL PRIVILEGES ON aquarium.* TO 'aquarium'@'localhost';

-- If accessing from remote machine (replace IP with your ESP32/server IP)
CREATE USER 'aquarium'@'192.168.1.%' IDENTIFIED BY 'your_password';
GRANT ALL PRIVILEGES ON aquarium.* TO 'aquarium'@'192.168.1.%';

-- Apply changes
FLUSH PRIVILEGES;

-- Verify
SHOW DATABASES;
SELECT User, Host FROM mysql.user WHERE User = 'aquarium';

-- Exit
EXIT;
```

### 4. Configure Firewall (if remote access needed)

```bash
# Allow MariaDB through firewall
sudo ufw allow 3306/tcp

# Or allow only from specific IP
sudo ufw allow from 192.168.1.0/24 to any port 3306
```

### 5. Enable Remote Connections (Optional)

Edit MariaDB configuration:
```bash
sudo nano /etc/mysql/mariadb.conf.d/50-server.cnf
```

Find and change:
```ini
# FROM:
bind-address = 127.0.0.1

# TO (allow all):
bind-address = 0.0.0.0

# OR (allow specific network):
bind-address = 192.168.1.100
```

Restart MariaDB:
```bash
sudo systemctl restart mariadb
```

### 6. Install Python Dependencies

```bash
cd /home/des/Documents/aquariumcontroller/tools/
pip3 install -r requirements.txt
```

This installs:
- `mysql-connector-python` - MariaDB driver
- `paho-mqtt` - MQTT client
- `numpy` - Numerical computing
- `scikit-learn` - Machine learning

### 7. Configure Environment Variables

```bash
cp .env.example .env
nano .env
```

Update with your settings:
```bash
# MQTT Broker (ESP32 location)
MQTT_BROKER=192.168.1.100
MQTT_PORT=1883
MQTT_USER=
MQTT_PASSWORD=

# MQTT Topic Prefix (must match ESP32)
MQTT_TOPIC_PREFIX=aquarium

# MariaDB Configuration
DB_HOST=localhost          # or remote IP
DB_PORT=3306
DB_NAME=aquarium
DB_USER=aquarium
DB_PASSWORD=your_password  # Set during Step 3
```

### 8. Test Database Connection

```bash
python3 -c "
import mysql.connector
import os
from dotenv import load_dotenv

load_dotenv()

try:
    conn = mysql.connector.connect(
        host=os.getenv('DB_HOST', 'localhost'),
        port=int(os.getenv('DB_PORT', 3306)),
        database=os.getenv('DB_NAME', 'aquarium'),
        user=os.getenv('DB_USER', 'aquarium'),
        password=os.getenv('DB_PASSWORD', '')
    )
    print('✅ Database connection successful!')
    conn.close()
except Exception as e:
    print(f'❌ Connection failed: {e}')
"
```

### 9. Initialize Database Tables

The ML service will automatically create tables on first run:

```bash
python3 water_change_ml_service.py
```

You should see:
```
INFO - Starting Aquarium Water Change ML Predictor Service
INFO - Connecting to MariaDB at localhost:3306/aquarium
INFO - MariaDB database initialized
INFO - Training initial model...
```

Press `Ctrl+C` after tables are created.

### 10. Verify Tables

```bash
mysql -u aquarium -p aquarium
```

```sql
-- Show all tables
SHOW TABLES;

-- Expected output:
-- +-------------------------+
-- | Tables_in_aquarium      |
-- +-------------------------+
-- | filter_maintenance      |
-- | predictions             |
-- | sensor_readings         |
-- | water_changes           |
-- +-------------------------+

-- View table structures
DESCRIBE sensor_readings;
DESCRIBE water_changes;
DESCRIBE filter_maintenance;
DESCRIBE predictions;

EXIT;
```

## Running the Service

### Option 1: Run Manually (Testing)

```bash
python3 tools/water_change_ml_service.py
```

### Option 2: Run as Systemd Service (Production)

Create service file:
```bash
sudo nano /etc/systemd/system/aquarium-ml.service
```

Content:
```ini
[Unit]
Description=Aquarium Water Change ML Predictor Service
After=network.target mariadb.service mosquitto.service
Requires=mariadb.service

[Service]
Type=simple
User=YOUR_USERNAME
WorkingDirectory=/home/des/Documents/aquariumcontroller/tools
Environment="PATH=/usr/local/bin:/usr/bin:/bin"
ExecStart=/usr/bin/python3 water_change_ml_service.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable aquarium-ml
sudo systemctl start aquarium-ml
sudo systemctl status aquarium-ml
```

### Option 3: Run via Cron (Daily Training)

```bash
crontab -e
```

Add:
```cron
# Run daily at 2 AM
0 2 * * * cd /home/des/Documents/aquariumcontroller/tools && /usr/bin/python3 train_and_predict.py >> /var/log/aquarium-ml.log 2>&1
```

## Database Maintenance

### Backup Database

```bash
# Full backup
mysqldump -u aquarium -p aquarium > aquarium_backup_$(date +%Y%m%d).sql

# Backup only data (no structure)
mysqldump -u aquarium -p --no-create-info aquarium > aquarium_data_backup.sql
```

### Restore Database

```bash
mysql -u aquarium -p aquarium < aquarium_backup_20251022.sql
```

### Monitor Database Size

```sql
SELECT 
    table_name AS "Table",
    ROUND(((data_length + index_length) / 1024 / 1024), 2) AS "Size (MB)"
FROM information_schema.TABLES
WHERE table_schema = "aquarium"
ORDER BY (data_length + index_length) DESC;
```

### Clean Old Sensor Data (Optional)

If database grows too large, keep only recent sensor readings:

```sql
-- Delete sensor readings older than 90 days
DELETE FROM sensor_readings 
WHERE timestamp < UNIX_TIMESTAMP(DATE_SUB(NOW(), INTERVAL 90 DAY));

-- Optimize table after deletion
OPTIMIZE TABLE sensor_readings;
```

**Note:** Never delete `water_changes` or `filter_maintenance` data - ML accuracy depends on historical records!

## Troubleshooting

### Connection Refused

**Error:** `Can't connect to MySQL server on 'localhost' (111)`

**Solutions:**
```bash
# Check if MariaDB is running
sudo systemctl status mariadb

# Start if stopped
sudo systemctl start mariadb

# Check if listening on correct port
sudo netstat -tlnp | grep 3306
```

### Access Denied

**Error:** `Access denied for user 'aquarium'@'localhost'`

**Solutions:**
```bash
# Verify user exists
sudo mysql -u root -p
```
```sql
SELECT User, Host FROM mysql.user WHERE User = 'aquarium';

-- If missing, recreate user
CREATE USER 'aquarium'@'localhost' IDENTIFIED BY 'your_password';
GRANT ALL PRIVILEGES ON aquarium.* TO 'aquarium'@'localhost';
FLUSH PRIVILEGES;
```

### Database Not Found

**Error:** `Unknown database 'aquarium'`

**Solution:**
```bash
sudo mysql -u root -p
```
```sql
CREATE DATABASE aquarium CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

### Slow Queries

**Problem:** Queries taking too long

**Solutions:**
```sql
-- Add indexes (if not exist)
CREATE INDEX idx_sensor_timestamp ON sensor_readings(timestamp);
CREATE INDEX idx_wc_timestamp ON water_changes(end_timestamp);
CREATE INDEX idx_filter_timestamp ON filter_maintenance(timestamp);

-- Analyze query performance
EXPLAIN SELECT * FROM sensor_readings WHERE timestamp > UNIX_TIMESTAMP(DATE_SUB(NOW(), INTERVAL 1 DAY));
```

### Remote Connection Issues

**Error:** Can't connect from remote machine

**Checklist:**
1. User created with correct host: `CREATE USER 'aquarium'@'%'` (or specific IP)
2. Firewall allows port 3306: `sudo ufw status`
3. MariaDB listening on correct interface: Check `bind-address` in `/etc/mysql/mariadb.conf.d/50-server.cnf`
4. Test from remote: `mysql -h 192.168.1.100 -u aquarium -p`

## Performance Optimization

### Enable Query Cache

```bash
sudo nano /etc/mysql/mariadb.conf.d/50-server.cnf
```

Add:
```ini
[mysqld]
query_cache_type = 1
query_cache_size = 16M
query_cache_limit = 2M
```

Restart:
```bash
sudo systemctl restart mariadb
```

### Tune Buffer Pool

For systems with more RAM:
```ini
[mysqld]
innodb_buffer_pool_size = 256M  # Adjust based on available RAM
```

### Monitor Performance

```sql
-- Show slow queries
SHOW VARIABLES LIKE 'slow_query%';

-- Enable slow query log
SET GLOBAL slow_query_log = 'ON';
SET GLOBAL long_query_time = 2;

-- View table statistics
SHOW TABLE STATUS FROM aquarium;
```

## Security Best Practices

1. **Strong passwords**: Use complex passwords for database users
2. **Limited privileges**: Don't use root for application
3. **Regular backups**: Automate daily backups
4. **Firewall rules**: Restrict database access to known IPs
5. **SSL/TLS**: Enable encrypted connections for remote access
6. **Update regularly**: Keep MariaDB updated with security patches

```bash
# Check for updates
sudo apt update
sudo apt list --upgradable | grep mariadb
```

## Data Retention Recommendations

| Table | Retention Period | Reason |
|-------|------------------|--------|
| `sensor_readings` | 30-90 days | Large volume, only recent data needed for predictions |
| `water_changes` | Forever | Critical for ML training and historical analysis |
| `filter_maintenance` | Forever | Important for understanding maintenance impact |
| `predictions` | Forever | Track ML accuracy over time |

## Advanced: Multi-Tank Setup

For multiple aquariums, you can:

### Option 1: Separate Databases
```sql
CREATE DATABASE aquarium_tank1;
CREATE DATABASE aquarium_tank2;
```

Run separate ML services with different `.env` files.

### Option 2: Single Database with Tank IDs

Modify tables to include `tank_id`:
```sql
ALTER TABLE sensor_readings ADD COLUMN tank_id VARCHAR(50) AFTER id;
ALTER TABLE water_changes ADD COLUMN tank_id VARCHAR(50) AFTER id;
-- Update queries to filter by tank_id
```

## Summary

You now have a production-ready MariaDB setup for your aquarium ML prediction system! The database will:

- ✅ Store all sensor readings, water changes, and filter maintenance
- ✅ Provide fast queries for ML training
- ✅ Support remote access from multiple clients
- ✅ Scale to millions of records
- ✅ Enable advanced analytics with SQL

For daily operations, just ensure:
1. MariaDB service is running
2. ML service is running (systemd or cron)
3. Regular backups are performed
4. Monitor disk space and database size

🐠 Happy aquarium keeping with AI-powered predictions!
