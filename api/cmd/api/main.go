package main

import (
	"github.com/valvosdev/aqua-eq8/api/infrastructure"
	"github.com/valvosdev/aqua-eq8/api/internal/server"
	"github.com/valvosdev/aqua-eq8/api/internal/storage"
	"github.com/valvosdev/aqua-eq8/api/internal/usecases"
	"go.uber.org/fx"
)

func main() {
	fx.New(
		// 1. Infrastructure
		infrastructure.Module,

		// 2. Storage/ internal staff
		storage.Module,

		// 3. Domain Layer Logic (Usecases / Services)
		// These will now automatically pick up the *sqlx.DB or *minio.Client they need.
		usecases.Module,

		// 4. HTTP Delivery Infrastructure Module
		server.Module,
	).Run()
}
