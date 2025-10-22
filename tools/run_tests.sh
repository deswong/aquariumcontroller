#!/bin/bash
# Quick Test Runner for Water Change ML Service
#
# This script provides an easy way to run all tests for the ML service

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "======================================================================"
echo "Water Change ML Service - Test Suite"
echo "======================================================================"
echo ""

# Check if we're in the right directory
if [ ! -f "test_ml_service.py" ]; then
    echo -e "${RED}Error: test_ml_service.py not found!${NC}"
    echo "Please run this script from the tools/ directory"
    exit 1
fi

# Check Python dependencies
echo -e "${YELLOW}Checking Python dependencies...${NC}"
python3 -c "import paho.mqtt.client" 2>/dev/null || {
    echo -e "${RED}Missing dependencies! Installing...${NC}"
    pip3 install -r requirements.txt
}
echo -e "${GREEN}✓ Dependencies OK${NC}"
echo ""

# Run unit tests
echo "======================================================================"
echo "Running Unit Tests (fast, no database required)"
echo "======================================================================"
echo ""
python3 test_ml_service.py

# Check if unit tests passed
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ All unit tests passed!${NC}"
    echo ""
    
    # Ask about integration tests
    echo "======================================================================"
    echo "Integration Tests"
    echo "======================================================================"
    echo ""
    echo "Integration tests require MariaDB to be running with a test database."
    echo ""
    read -p "Do you want to run integration tests? (y/n) " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        # Check if .env.test exists
        if [ ! -f ".env.test" ]; then
            echo -e "${YELLOW}Warning: .env.test not found${NC}"
            echo "Creating from .env.test.example..."
            cp .env.test.example .env.test
            echo ""
            echo -e "${YELLOW}Please edit .env.test with your MariaDB credentials:${NC}"
            echo "  nano .env.test"
            echo ""
            echo "Then run integration tests with:"
            echo "  python3 test_ml_integration.py"
            exit 0
        fi
        
        echo ""
        echo "Running integration tests..."
        python3 test_ml_integration.py
        
        if [ $? -eq 0 ]; then
            echo ""
            echo -e "${GREEN}✓ All integration tests passed!${NC}"
            echo ""
            echo "======================================================================"
            echo -e "${GREEN}ALL TESTS PASSED - ML SERVICE IS READY FOR PRODUCTION${NC}"
            echo "======================================================================"
        else
            echo ""
            echo -e "${RED}✗ Integration tests failed${NC}"
            echo "Check the output above for details"
            exit 1
        fi
    else
        echo ""
        echo "Skipping integration tests"
        echo ""
        echo "To run integration tests later:"
        echo "  1. Set up test database (see MARIADB_SETUP_GUIDE.md)"
        echo "  2. Configure .env.test"
        echo "  3. Run: python3 test_ml_integration.py"
    fi
else
    echo ""
    echo -e "${RED}✗ Unit tests failed${NC}"
    echo "Check the output above for details"
    exit 1
fi
