package postgres

// Filter defines a matching pair for the database WHERE clause.
type Filter struct {
	Field    string      // Database column name (e.g., "username")
	Operator string      // Operator like "=", "LIKE", ">", "IN"
	Value    interface{} // The value to query against
}

// QueryOptions consolidates pagination and active filtering rules.
type QueryOptions struct {
	Page    int      // Page index (starting at 1)
	Limit   int      // Max records per page
	Filters []Filter // Dynamic array of where constraints
}

// PaginatedResult wraps the query payload with transactional metadata.
type PaginatedResult[T any] struct {
	Data       []T   `json:"data"`
	TotalCount int64 `json:"total_count"`
	TotalPages int   `json:"total_pages"`
	Page       int   `json:"page"`
	Limit      int   `json:"limit"`
}
