package db

import (
	"fmt"

	"time"

	"github.com/valvosdev/aqua-eq8/api/infrastructure/config"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

func NewPostgres(cfg *config.Config) (*gorm.DB, error) {
	dns := fmt.Sprintf("host=%s user=%s password=%s dbname=%s port=%s sslmode=%s",
		cfg.Db.Host,
		cfg.Db.User,
		cfg.Db.Password,
		cfg.Db.DbName,
		cfg.Db.Port,
		cfg.Db.SSLMode)

	postgresConfig := postgres.Config{
		DSN: dns,
	}

	gormConfig := gorm.Config{
		SkipDefaultTransaction: true,
		DryRun:                 false,
		PrepareStmt:            true,
	}

	db, err := gorm.Open(postgres.New(postgresConfig), &gormConfig)
	if err != nil {
		return nil, err
	}

	//connection pool config
	if sqlDb, err := db.DB(); err == nil {
		sqlDb.SetMaxIdleConns(cfg.Db.MaxIdleConns)
		sqlDb.SetMaxOpenConns(cfg.Db.MaxOpenConns)
		sqlDb.SetConnMaxLifetime(time.Hour)
	}

	return db, nil

}
