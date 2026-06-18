package storage

import (
	"github.com/valvosdev/aqua-eq8/api/internal/domain/user"
	"github.com/valvosdev/aqua-eq8/api/internal/storage/postgres"
	"go.uber.org/fx"
	"gorm.io/gorm"
)

var Module = fx.Module(
	"storage",
	fx.Provide(
		As(postgres.NewRepository[user.User]),
	),
)

// As constructor wraps the generic repository factory into a type-safe signature for Uber/Fx
func As[TEntity postgres.Entity](factory func(*gorm.DB) postgres.Repository[TEntity]) any {
	return func(db *gorm.DB) postgres.Repository[TEntity] {
		return factory(db)
	}
}
