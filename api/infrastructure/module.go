package infrastructure

import (
	"context"
	"fmt"

	"github.com/valvosdev/aqua-eq8/api/infrastructure/config"
	"github.com/valvosdev/aqua-eq8/api/infrastructure/db"
	"go.uber.org/fx"
	"gorm.io/gorm"
)

var Module = fx.Module("infrastructure",
	fx.Provide(
		// Load Configuration
		config.NewConfig,

		// Initialize Postgres & Register its cleanup hook
		func(lc fx.Lifecycle, cfg *config.Config) (*gorm.DB, error) {
			pgDb, err := db.NewPostgres(cfg)
			if err != nil {
				return nil, fmt.Errorf("failed to connect to DB: %w", err)
			}
			sqlDb, err := pgDb.DB()
			if err != nil {
				return nil, fmt.Errorf("failed to extract SQL DB from GORM: %w", err)
			}

			// Enforce Clean Architecture: Close DB connection pool on app shutdown
			lc.Append(fx.Hook{
				OnStop: func(ctx context.Context) error {
					return sqlDb.Close()
				},
			})
			return pgDb, nil
		},
	),
)
